"""Exercise the compiled CLI, including capacity and malformed-input boundaries."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class LogAnalysisTests(unittest.TestCase):
    def analyze(self, content, *arguments, expected=0):
        result = subprocess.run([BINARY, '--json', *arguments], input=content, capture_output=True)
        self.assertEqual(result.returncode, expected, result.stderr)
        return json.loads(result.stdout)

    def test_levels_templates_and_unterminated_last_line(self):
        report = self.analyze(b'[ERROR] request 123 failed\n[error] request 987 failed\nINFO ok\nother')
        self.assertEqual(report['lines'], 4)
        self.assertEqual(report['levels']['ERROR'], 2)
        self.assertEqual(report['levels']['UNKNOWN'], 1)
        self.assertEqual(report['patterns'][0]['count'], 2)
        self.assertEqual(report['patterns'][0]['template'], '[error] request # failed')

    def test_severity_is_whole_token(self):
        report = self.analyze(b'information terror INFO_abc\nWARNING retry\nCRITICAL disk')
        self.assertEqual(report['levels']['UNKNOWN'], 1)
        self.assertEqual(report['levels']['WARN'], 1)
        self.assertEqual(report['levels']['FATAL'], 1)

    def test_filters_do_not_change_global_severity_counts(self):
        report = self.analyze(b'INFO api\nERROR api\nERROR database\nunknown api', '--min-level', 'WARN', '--contains', 'api')
        self.assertEqual(report['matched'], 1)
        self.assertEqual(report['levels']['ERROR'], 2)

    def test_binary_and_oversized_lines_are_skipped_and_stream_recovers(self):
        report = self.analyze(b'ERROR ' + b'x' * 100 + b'\nabc\x00def\nINFO ok\n', '--max-line', '20', expected=1)
        self.assertEqual(report['lines'], 3)
        self.assertEqual(report['skipped_long'], 1)
        self.assertEqual(report['skipped_binary'], 1)
        self.assertEqual(report['matched'], 1)
        self.assertFalse(report['report_complete'])

    def test_capacity_still_counts_existing_patterns(self):
        report = self.analyze(b'ERROR first\nERROR second\nERROR first', '--max-patterns', '1', expected=1)
        self.assertEqual(report['untracked_patterns'], 1)
        self.assertEqual(report['patterns'][0]['count'], 2)
        self.assertEqual(report['matched'], 3)

    def test_empty_input_and_blank_lines(self):
        self.assertEqual(self.analyze(b'')['lines'], 0)
        self.assertEqual(self.analyze(b'\n\n')['lines'], 2)

    def test_json_escape_arbitrary_bytes(self):
        report = self.analyze(b'ERROR "quoted" \\ path \xff\n')
        self.assertIn('"quoted"', report['patterns'][0]['template'])
        self.assertIn('ÿ', report['patterns'][0]['template'])

    def test_template_truncation_is_explicit(self):
        report = self.analyze(b'ERROR ' + b'x' * 1000)
        self.assertTrue(report['patterns'][0]['truncated'])

    def test_valid_utf8_is_preserved(self):
        report = self.analyze('ERROR اتصال مفقود\n'.encode('utf-8'))
        self.assertIn('اتصال مفقود', report['patterns'][0]['template'])

    def test_control_z_does_not_end_windows_stdin(self):
        report = self.analyze(b'INFO before\nDEBUG control\x1a byte\nERROR after\n')
        self.assertEqual(report['lines'], 3)
        self.assertEqual(report['levels']['ERROR'], 1)

    def test_multiple_files_aggregate_and_missing_file_is_visible(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / 'a.log', Path(directory) / 'b.log'
            a.write_text('ERROR request 1\n')
            b.write_text('ERROR request 2\n')
            result = subprocess.run([BINARY, '--json', str(a), str(b)], capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(json.loads(result.stdout)['patterns'][0]['count'], 2)
            result = subprocess.run([BINARY, '--json', str(a), str(b) + '.missing'], capture_output=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b'missing', result.stderr)

    def test_fail_on_error(self):
        self.analyze(b'ERROR failed', '--fail-on-error', expected=3)
        self.analyze(b'INFO okay', '--fail-on-error')

    def test_unicode_file_argument(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / 'سجل.log'
            file.write_text('ERROR اختبار\n', encoding='utf-8')
            result = subprocess.run([BINARY, '--json', str(file)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('اختبار', json.loads(result.stdout)['patterns'][0]['template'])

    def test_invalid_configuration(self):
        for args in [['--top', '0'], ['--max-line', '-1'], ['--max-patterns', '100001'], ['--min-level', 'OTHER'], ['--contains'], ['--unknown']]:
            result = subprocess.run([BINARY, *args], input=b'', capture_output=True)
            self.assertEqual(result.returncode, 2, args)


if __name__ == '__main__':
    unittest.main()
