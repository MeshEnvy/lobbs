#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSReply.h"
#include "LoBBSResponse.h"
#include "protocol/machine/paginate.h"
#include "protocol/machine/serialize.h"
#include "protocol/plain_text/paginate.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <string>
#include <unity.h>
#include <vector>

static std::string lobbsTestMachineFragmentBody(const std::string &frag)
{
    size_t nl = frag.find('\n');
    if (nl == std::string::npos)
        return "";
    return frag.substr(nl + 1);
}

static void test_protocol_plain_multi_page_footer()
{
    std::string text;
    for (int i = 0; i < 20; i++)
        text += "line " + std::to_string(i) + " filler text here\n";
    std::string page;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginatePlainText(text, 1, page, &err));
    TEST_ASSERT_LESS_OR_EQUAL(LOBBS_REPLY_BYTES, page.size());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, page.find("{p 1/"));
}

static void test_protocol_plain_single_page_no_footer()
{
    std::string text = "one line only";
    std::string page;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginatePlainText(text, 1, page, &err));
    TEST_ASSERT_EQUAL_STRING("one line only", page.c_str());
    TEST_ASSERT_NULL(strstr(page.c_str(), "{p "));
}

static void test_protocol_plain_long_line_truncated()
{
    std::string text(250, 'x');
    std::string page;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginatePlainText(text, 1, page, &err));
    TEST_ASSERT_LESS_OR_EQUAL(LOBBS_REPLY_BYTES, page.size());
    TEST_ASSERT_NOT_NULL(strstr(page.c_str(), "[...]"));
}

static void test_protocol_plain_no_such_page()
{
    std::string text = "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\n";
    std::string page;
    const char *err = nullptr;
    TEST_ASSERT_FALSE(lobbsPaginatePlainText(text, 99, page, &err));
    TEST_ASSERT_EQUAL_STRING("No such page.", err);
}

static void test_protocol_plain_empty()
{
    std::string page;
    const char *err = nullptr;
    TEST_ASSERT_FALSE(lobbsPaginatePlainText("", 1, page, &err));
    TEST_ASSERT_EQUAL_STRING("Empty.", err);
}

static void test_protocol_machine_fragments_fit_budget()
{
    std::string doc(700, 'x');
    for (uint32_t n = 1; n <= 32; n++) {
        std::string frag;
        const char *err = nullptr;
        if (!lobbsPaginateMachine(42, doc, n, frag, &err))
            break;
        TEST_ASSERT_LESS_OR_EQUAL(LOBBS_REPLY_BYTES, frag.size());
    }
}

static void test_protocol_machine_last_fragment_marker()
{
    std::string doc(700, 'y');
    uint32_t last = 0;
    for (uint32_t n = 1; n <= 32; n++) {
        std::string frag;
        const char *err = nullptr;
        if (!lobbsPaginateMachine(7, doc, n, frag, &err))
            break;
        last = n;
    }
    TEST_ASSERT_TRUE(last > 1);
    std::string lastFrag;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginateMachine(7, doc, last, lastFrag, &err));
    char header[32];
    snprintf(header, sizeof(header), "<7>ok [%u:%u]\n", last, last);
    TEST_ASSERT_EQUAL_STRING_LEN(header, lastFrag.c_str(), strlen(header));
}

static void test_protocol_machine_reassemble_document()
{
    std::string doc(700, 'z');
    std::string joined;
    for (uint32_t n = 1;; n++) {
        std::string frag;
        const char *err = nullptr;
        if (!lobbsPaginateMachine(42, doc, n, frag, &err))
            break;
        joined += lobbsTestMachineFragmentBody(frag);
    }
    TEST_ASSERT_EQUAL_STRING(doc.c_str(), joined.c_str());
}

static void test_protocol_machine_single_no_double_ok()
{
    LoBBSResponse resp;
    LoScalar rec;
    rec.setString(LODB_F_TITLE, "hello");
    resp.records.push_back(rec);
    std::string doc;
    TEST_ASSERT_TRUE(lobbsSerializeMachine(resp, doc));
    TEST_ASSERT_NULL(strstr(doc.c_str(), "ok"));
    std::string frag;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginateMachine(99, doc, 1, frag, &err));
    TEST_ASSERT_NOT_NULL(strstr(frag.c_str(), "<99>ok\n"));
    std::string body = lobbsTestMachineFragmentBody(frag);
    TEST_ASSERT_FALSE(body.rfind("ok\n", 0) == 0);
}

static void test_protocol_machine_req_id_in_header()
{
    std::string doc(400, 'a');
    std::string f1;
    std::string f2;
    const char *err = nullptr;
    TEST_ASSERT_TRUE(lobbsPaginateMachine(1, doc, 1, f1, &err));
    TEST_ASSERT_TRUE(lobbsPaginateMachine(999, doc, 1, f2, &err));
    TEST_ASSERT_NOT_NULL(strstr(f1.c_str(), "<1>ok [1:"));
    TEST_ASSERT_NOT_NULL(strstr(f2.c_str(), "<999>ok [1:"));
}

static void test_protocol_machine_serialize_error()
{
    LoBBSResponse resp;
    resp.ok = false;
    resp.error = "Nope.";
    std::string out;
    TEST_ASSERT_TRUE(lobbsSerializeMachine(resp, out));
    TEST_ASSERT_EQUAL_STRING("Nope.", out.c_str());
}

static void test_protocol_machine_serialize_escapes_and_fields()
{
    LoScalar rec;
    rec.setString(LODB_F_ID, "id1");
    rec.setString(LODB_F_TITLE, "a|b\\c\nd");
    LoBBSResponse resp;
    resp.records.push_back(rec);
    std::string line;
    TEST_ASSERT_TRUE(lobbsSerializeMachine(resp, line));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "99:id1"));
    TEST_ASSERT_NOT_NULL(strstr(line.c_str(), "98:a\\|b\\\\c\\nd"));
    TEST_ASSERT_TRUE(line.find("98:") < line.find("99:"));
}

inline void lobbsRunProtocolTests()
{
    RUN_TEST(test_protocol_plain_multi_page_footer);
    RUN_TEST(test_protocol_plain_single_page_no_footer);
    RUN_TEST(test_protocol_plain_long_line_truncated);
    RUN_TEST(test_protocol_plain_no_such_page);
    RUN_TEST(test_protocol_plain_empty);
    RUN_TEST(test_protocol_machine_fragments_fit_budget);
    RUN_TEST(test_protocol_machine_last_fragment_marker);
    RUN_TEST(test_protocol_machine_reassemble_document);
    RUN_TEST(test_protocol_machine_single_no_double_ok);
    RUN_TEST(test_protocol_machine_req_id_in_header);
    RUN_TEST(test_protocol_machine_serialize_error);
    RUN_TEST(test_protocol_machine_serialize_escapes_and_fields);
}

#endif
