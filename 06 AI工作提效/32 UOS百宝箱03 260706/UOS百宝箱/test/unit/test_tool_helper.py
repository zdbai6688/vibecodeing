#!/usr/bin/env python3
"""单元测试: tool_helper.py"""
import sys
import os
import json
import unittest
from unittest.mock import patch, MagicMock
from io import StringIO

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../resources'))

class TestToolHelper(unittest.TestCase):
    """测试 tool_helper.py 中各命令的处理逻辑"""

    def setUp(self):
        self.helper_path = os.path.join(
            os.path.dirname(__file__), '../../resources/tool_helper.py'
        )

    def test_script_exists(self):
        """验证 tool_helper.py 文件存在且可读"""
        self.assertTrue(os.path.isfile(self.helper_path))
        with open(self.helper_path, 'r') as f:
            content = f.read()
        self.assertIn('def main()', content)
        self.assertIn('image-resize', content)
        self.assertIn('image-convert', content)
        self.assertIn('ocr', content)
        self.assertIn('scan-effect', content)
        self.assertIn('query-cve', content)

    def test_syntax_valid(self):
        """验证 Python 语法正确"""
        import py_compile
        try:
            py_compile.compile(self.helper_path, doraise=True)
            syntax_ok = True
        except py_compile.PyCompileError:
            syntax_ok = False
        self.assertTrue(syntax_ok)

    def test_check_tesseract_installed(self):
        """测试 check_tesseract 成功时返回 True"""
        with patch('subprocess.run') as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            from tool_helper import check_tesseract
            self.assertTrue(check_tesseract())

    def test_check_tesseract_not_installed(self):
        """测试 check_tesseract 失败时返回 False"""
        with patch('subprocess.run') as mock_run:
            mock_run.return_value = MagicMock(returncode=1)
            from tool_helper import check_tesseract
            self.assertFalse(check_tesseract())

    def test_check_tesseract_exception(self):
        """测试 check_tesseract 异常时返回 False"""
        with patch('subprocess.run') as mock_run:
            mock_run.side_effect = FileNotFoundError()
            from tool_helper import check_tesseract
            self.assertFalse(check_tesseract())

    def test_install_tesseract_success(self):
        """测试 install_tesseract 成功"""
        with patch('subprocess.run') as mock_run:
            mock_run.return_value = MagicMock(returncode=0)
            from tool_helper import install_tesseract
            self.assertTrue(install_tesseract())

    def test_install_tesseract_failure(self):
        """测试 install_tesseract 失败"""
        with patch('subprocess.run') as mock_run:
            mock_run.side_effect = Exception('apt-get failed')
            from tool_helper import install_tesseract
            self.assertFalse(install_tesseract())

    def _run_main(self, args):
        """Helper: run main() with given argv and return parsed JSON output"""
        old_stdout = sys.stdout
        redirected = StringIO()
        sys.stdout = redirected
        try:
            with patch.object(sys, 'argv', args):
                from tool_helper import main
                main()
        finally:
            sys.stdout = old_stdout
        output = redirected.getvalue().strip()
        if not output:
            return {"success": False, "error": "no output"}
        return json.loads(output)

    def test_main_no_args(self):
        """测试 main() 无参数时返回错误"""
        result = self._run_main(['tool_helper.py'])
        self.assertFalse(result['success'])
        self.assertIn('缺少参数', result['error'])

    def test_main_unknown_command(self):
        """测试 main() 未知命令时返回错误"""
        result = self._run_main(['tool_helper.py', 'unknown-cmd', '{}'])
        self.assertFalse(result['success'])

    def test_main_query_cve_empty(self):
        """测试 main() query-cve 空参数时返回错误"""
        result = self._run_main(['tool_helper.py', 'query-cve', '{}'])
        self.assertFalse(result['success'])
        self.assertIn('请提供 CVE', result['error'])

    @patch('tool_helper.check_tesseract')
    def test_main_ocr_no_tesseract(self, mock_check):
        """测试 main() ocr 且 tesseract 未安装时返回 need_install=True"""
        mock_check.return_value = False
        result = self._run_main(['tool_helper.py', 'ocr', '{}'])
        self.assertFalse(result['success'])
        self.assertTrue(result.get('need_install', False))

    def test_main_install_tesseract(self):
        """测试 main() install-tesseract 命令"""
        result = self._run_main(['tool_helper.py', 'install-tesseract', '{}'])
        self.assertFalse(result['success'])
        self.assertIn('sudo apt-get install', result['error'])

    @patch('tool_helper.check_tesseract')
    def test_main_check_tesseract(self, mock_check):
        """测试 main() check-tesseract 命令"""
        mock_check.return_value = True
        result = self._run_main(['tool_helper.py', 'check-tesseract', '{}'])
        self.assertTrue(result['success'])
        self.assertTrue(result['installed'])

    def test_main_image_resize_no_file(self):
        """测试 main() image-resize 无文件时返回错误"""
        result = self._run_main(['tool_helper.py', 'image-resize', '{}'])
        self.assertFalse(result['success'])

    def test_main_image_convert_no_file(self):
        """测试 main() image-convert 无文件时返回错误"""
        result = self._run_main(['tool_helper.py', 'image-convert', '{}'])
        self.assertFalse(result['success'])

    @patch('urllib.request.urlopen')
    def test_main_query_cve_network_error(self, mock_urlopen):
        """测试 main() query-cve 网络异常时返回错误"""
        mock_urlopen.side_effect = Exception('Network error')
        result = self._run_main(['tool_helper.py', 'query-cve', '"CVE-2024-9999"'])
        self.assertFalse(result['success'])


    def test_main_scan_effect_no_file(self):
        """测试 main() scan-effect 无文件时返回错误"""
        result = self._run_main(['tool_helper.py', 'scan-effect', '{}'])
        self.assertFalse(result['success'])


if __name__ == '__main__':
    unittest.main()
