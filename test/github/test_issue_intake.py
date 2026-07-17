import base64
import json
import os
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

import yaml


ROOT = Path(__file__).resolve().parents[2]
TEMPLATE_DIR = ROOT / ".github" / "ISSUE_TEMPLATE"
REPOSITORY = "Sakyvo/OBS-PT"
FORMS = [
    ("01-bug.yml", "错误报告 / Bug report", "[bug] ", ["bug", "triage"]),
    (
        "02-enhancement.yml",
        "功能建议 / Enhancement",
        "[enhancement] ",
        ["enhancement", "triage"],
    ),
    ("03-help.yml", "使用求助 / Help", "[help] ", ["help", "triage"]),
    (
        "04-discussion.yml",
        "开放讨论 / Discussion",
        "[discussion] ",
        ["discussion", "triage"],
    ),
]
SYSTEM_FIELDS = [
    ("obspt-version", "OBS-PT 版本 / OBS-PT version"),
    ("operating-system", "操作系统 / Operating system"),
    ("cpu", "CPU"),
    ("gpu-driver", "GPU 与驱动版本 / GPU and driver version"),
    ("memory", "内存 / Memory"),
    ("recording-drive", "录像磁盘 / Recording drive"),
]
COMMON_CONFIRMATIONS = [
    "我已搜索现有 Issue，确认没有相同内容 / "
    "I searched existing issues and found no duplicate.",
    "我确认事项针对 OBS-PT，而非上游 OBS Studio 或无关第三方软件 / "
    "I confirm this concerns OBS-PT, not upstream OBS Studio or unrelated "
    "third-party software.",
    "我确认填写的信息准确 / I confirm the information is accurate.",
    "我已检查公开上传的截图、日志和路径是否含敏感信息 / "
    "I checked public screenshots, logs, and paths for sensitive information.",
]
EXPECTED_LABELS = {
    "bug": ("d73a4a", "功能异常 / Something is not working"),
    "enhancement": ("a2eeef", "功能改进或新增请求 / Improvement or feature request"),
    "help": ("0075ca", "OBS-PT 使用求助 / Usage help for OBS-PT"),
    "discussion": ("d876e3", "开放讨论主题 / Topic for open discussion"),
    "triage": ("fbca04", "等待维护者审核 / Awaiting maintainer review"),
    "wip": ("1d76db", "维护者正在处理 / Work in progress"),
    "duplicate": (
        "cfd3d7",
        "已有相同 Issue，将关闭 / Duplicate of an existing issue; will be closed",
    ),
    "wontfix": (
        "6e7781",
        "不计划处理，将关闭 / Will not be worked on; will be closed",
    ),
}


def load_form(filename):
    return yaml.safe_load((TEMPLATE_DIR / filename).read_text("utf-8"))


def fields(filename):
    return [item for item in load_form(filename)["body"] if item["type"] != "markdown"]


def field_contract(filename, start, stop):
    return [
        (
            item.get("id"),
            item.get("attributes", {}).get("label"),
            item.get("type"),
            item.get("validations", {}).get("required", False),
        )
        for item in fields(filename)[start:stop]
    ]


def github_json(url):
    request = Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "OBS-PT-issue-intake-test",
        },
    )
    with urlopen(request, timeout=30) as response:
        return json.load(response)


class LocalIssueIntakeTests(unittest.TestCase):
    def test_contributors_get_four_ordered_forms_without_blank_issues(self):
        actual_forms = []
        for filename, _, _, _ in FORMS:
            form = load_form(filename)
            actual_forms.append(
                (
                    filename,
                    form.get("name"),
                    form.get("title"),
                    form.get("labels"),
                    bool(form.get("description", "").strip() and form.get("body")),
                )
            )
        self.assertEqual(
            {
                "config": load_form("config.yml"),
                "forms": actual_forms,
            },
            {
                "config": {"blank_issues_enabled": False, "contact_links": []},
                "forms": [(*form, True) for form in FORMS],
            },
        )

    def test_every_form_requires_the_six_system_information_fields(self):
        expected = [
            (field_id, label, "input", True) for field_id, label in SYSTEM_FIELDS
        ]
        for filename, _, _, _ in FORMS:
            with self.subTest(filename=filename):
                self.assertEqual(field_contract(filename, 0, 6), expected)

    def test_bug_reports_require_reproduction_and_diagnostic_responses(self):
        expected = [
            ("actual-behavior", "实际发生了什么 / What happened?"),
            ("reproduction-steps", "如何复现 / Reproduction steps"),
            ("expected-behavior", "期望发生什么 / Expected behavior"),
            (
                "obs-statistics",
                "OBS 统计截图或原因 / OBS Statistics screenshot or reason",
            ),
            (
                "task-manager-usage",
                "任务管理器 CPU、GPU 占用截图或原因 / "
                "Task Manager CPU/GPU screenshot or reason",
            ),
            ("log-or-reason", "日志或无法提供的原因 / Log or reason unavailable"),
        ]
        self.assertEqual(
            field_contract("01-bug.yml", 6, 12),
            [(field_id, label, "textarea", True) for field_id, label in expected],
        )

    def test_bug_diagnostic_fields_explain_attachment_or_reason_options(self):
        descriptions = {
            item.get("id"): item.get("attributes", {}).get("description", "")
            for item in load_form("01-bug.yml")["body"]
        }
        required_fragments = {
            "obs-statistics": (
                "上传",
                "Upload",
                "无法提供",
                "unavailable",
                "不适用",
                "not applicable",
            ),
            "task-manager-usage": (
                "CPU、GPU",
                "CPU/GPU",
                "无法提供",
                "unavailable",
                "不适用",
                "not applicable",
            ),
            "log-or-reason": (
                ".txt",
                "<Install Root>/obs-studio/logs/",
                "帮助 → 日志文件",
                "Help → Log Files",
                "同次运行",
                "same run",
                "具体原因",
                "specific reason",
            ),
        }
        missing = {
            field_id: [
                fragment
                for fragment in fragments
                if fragment not in descriptions.get(field_id, "")
            ]
            for field_id, fragments in required_fragments.items()
        }
        self.assertEqual(missing, {field_id: [] for field_id in required_fragments})

    def test_enhancement_requests_collect_the_agreed_decision_context(self):
        expected = [
            (
                "current-limitation",
                "当前问题或限制 / Current problem or limitation",
                True,
            ),
            (
                "desired-improvement",
                "期望的改进及结果 / Desired improvement and outcome",
                True,
            ),
            (
                "potpvp-use-case",
                "PotPvP 录制使用场景 / PotPvP recording use case",
                True,
            ),
            (
                "workarounds",
                "替代方案或变通方法 / Alternatives or workarounds",
                False,
            ),
            ("supporting-material", "补充材料 / Supporting material", False),
        ]
        self.assertEqual(
            field_contract("02-enhancement.yml", 6, 11),
            [
                (field_id, label, "textarea", required)
                for field_id, label, required in expected
            ],
        )

    def test_help_requests_collect_the_goal_and_attempted_solutions(self):
        expected = [
            ("help-needed", "需要什么帮助 / What do you need help with?", True),
            ("desired-result", "希望达到的结果 / Desired result", True),
            ("attempted-methods", "已尝试的方法 / What have you tried?", True),
            (
                "supporting-material",
                "相关配置、截图或日志 / "
                "Relevant configuration, screenshots, or logs",
                False,
            ),
        ]
        self.assertEqual(
            field_contract("03-help.yml", 6, 10),
            [
                (field_id, label, "textarea", required)
                for field_id, label, required in expected
            ],
        )

    def test_discussions_collect_context_position_and_desired_conclusion(self):
        expected = [
            ("topic", "讨论主题 / Discussion topic", True),
            (
                "background",
                "背景及与 OBS-PT、PotPvP 录制的关系 / "
                "Background and relation to OBS-PT and PotPvP recording",
                True,
            ),
            (
                "current-position",
                "当前观点或初步方案 / Current position or proposal",
                True,
            ),
            (
                "desired-conclusion",
                "希望解决的问题或达成的结论 / Desired question or conclusion",
                True,
            ),
            (
                "alternatives-references",
                "其他方案、权衡或参考资料 / "
                "Alternatives, tradeoffs, or references",
                False,
            ),
        ]
        self.assertEqual(
            field_contract("04-discussion.yml", 6, 11),
            [
                (field_id, label, "textarea", required)
                for field_id, label, required in expected
            ],
        )

    def test_every_form_requires_the_submission_confirmations(self):
        expected_by_file = {
            "01-bug.yml": COMMON_CONFIRMATIONS
            + [
                "我已尽可能在当前 OBS-PT 版本中复现问题 / "
                "I reproduced this on the current OBS-PT version where possible."
            ],
            "02-enhancement.yml": COMMON_CONFIRMATIONS,
            "03-help.yml": COMMON_CONFIRMATIONS,
            "04-discussion.yml": COMMON_CONFIRMATIONS,
        }
        for filename, expected in expected_by_file.items():
            with self.subTest(filename=filename):
                checklists = [
                    item
                    for item in load_form(filename)["body"]
                    if item["type"] == "checkboxes"
                ]
                actual = []
                if len(checklists) == 1:
                    actual = [
                        (option.get("label"), option.get("required"))
                        for option in checklists[0]["attributes"]["options"]
                    ]
                self.assertEqual(actual, [(label, True) for label in expected])


@unittest.skipUnless(
    os.environ.get("OBS_PT_VERIFY_REMOTE") == "1",
    "set OBS_PT_VERIFY_REMOTE=1 to inspect GitHub",
)
class RemoteIssueIntakeTests(unittest.TestCase):
    def test_live_repository_has_exactly_the_agreed_label_taxonomy(self):
        labels = github_json(
            f"https://api.github.com/repos/{REPOSITORY}/labels?per_page=100"
        )
        actual = {
            label["name"]: (label["color"].lower(), label.get("description") or "")
            for label in labels
        }
        self.assertEqual(actual, EXPECTED_LABELS)

    def test_default_branch_publishes_the_tested_forms_and_blank_issue_policy(self):
        filenames = ["config.yml", *[form[0] for form in FORMS]]
        published = {}
        for filename in filenames:
            url = (
                f"https://api.github.com/repos/{REPOSITORY}/contents/"
                f".github/ISSUE_TEMPLATE/{filename}?ref=master"
            )
            try:
                payload = github_json(url)
                published[filename] = base64.b64decode(payload["content"]).decode(
                    "utf-8"
                )
            except HTTPError as error:
                try:
                    if error.code != 404:
                        raise
                    published[filename] = None
                finally:
                    error.close()

        expected = {
            filename: (TEMPLATE_DIR / filename).read_text("utf-8")
            for filename in filenames
        }
        self.assertEqual(published, expected)


if __name__ == "__main__":
    unittest.main()
