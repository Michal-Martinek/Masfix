from subprocess import Popen, PIPE
import os, sys
import re
from pathlib import Path

class TestcaseException(Exception):
	pass
def quoted(s):
	return "'" + str(s) + "'"
def check(cond, *messages, insideTestcase=True): # TODO use *messages, use properly everywhere
	if not cond:
		print('ERROR:', *messages)
		if insideTestcase:
			raise TestcaseException(*messages)
		else:
			exit(1)
# testcases ------------------------------------
def runCommand(command):
	process = Popen(command, stdout=PIPE, stderr=PIPE) # TODO handle process errors
	stdout, stderr = process.communicate()
	stdout = stdout.decode().replace('\r', '') # TODO handle decode errors
	stderr = stderr.decode().replace('\r', '')
	return process.returncode, stdout, stderr
def _parseDescBlob(desc, chars):
	check('\n' in desc, 'Testcase blob must start at a new line')
	s = desc[desc.find('\n')+1:]
	check(len(s) >= chars, 'Insufficent character count in description')
	return s[:chars]
def parseTestcaseDesc(desc: str):
	expected = {'stdout': '', 'stderr': ''}
	while ':' in desc:
		desc = desc[desc.find(':')+1:]
		line = desc.split('\n', maxsplit=1)[0]
		desc = desc[len(line):]
		if re.match('returncode \d+', line):
			code = int(line.split(' ')[1])
			expected['returncode'] = code
		elif re.match('stdout \d+', line):
			chars = int(line.split(' ')[1])
			s = _parseDescBlob(desc, chars)
			expected['stdout'] = s
			desc = desc[chars:]
		elif re.match('stderr \d+', line):
			chars = int(line.split(' ')[1])
			s = _parseDescBlob(desc, chars)
			expected['stderr'] = s
			desc = desc[chars:]
		else:
			check(False, 'Unknown field in description', quoted(line))
	check('returncode' in expected, 'Testcase description missing :returncode')
	return expected
def getTestcaseDesc(test: Path) -> dict:
	path = test.with_suffix('.txt')
	check(os.path.exists(path), 'Missing test case desciption', quoted(path))
	with open(path, 'r') as testcase:
		desc = testcase.read()
	return parseTestcaseDesc(desc)
def runFile(prompt, path) -> tuple:
	print(prompt, path)
	return runCommand(['Masfix',  '-s',  '-r', str(path)])
def runTest(path: Path) -> bool:
	code, stdout, stderr = runFile('[TESTING]', path)
	expected = getTestcaseDesc(path)
	check(expected['returncode'] == code, 'The return code is not as expected')
	check(expected['stdout'] == stdout, 'The stdout is not as expected')
	check(expected['stderr'] == stderr, 'The stderr is not as expected')
	return True
def runTests(dir: Path):
	success = True
	for file in os.listdir(dir):
		path = Path(os.path.join(dir, file))
		if path.suffix != '.mx':
			continue
		try:
			passed = runTest(path)
		except TestcaseException:
			passed = False
		success = success and passed
	print()
	if success:
		print('All testcases passed')
	else:
		print('Some testcases failed')
		exit(1)
def genDesc(file: Path, code, stdout, stderr):
	with open(file.with_suffix('.txt'), 'w') as f:
		f.write(f':returncode {code}\n\n')
		if stdout: f.write(f':stdout {len(stdout)}\n{stdout}\n\n')
		if stderr: f.write(f':stderr {len(stderr)}\n{stderr}\n\n')
def updateFile(file: Path):
	code, stdout, stderr = runFile('[UPDATING]', file)
	print('[NOTE] returncode:', code)
	print('[NOTE] stdout:')
	print(stdout)
	print('[NOTE] stderr:')
	print(stderr, end='')
	genDesc(file, code, stdout, stderr)
# modes --------------------------------------
def processFileArg(arg) -> Path:
	file = None
	for folder in ['', 'tests']:
		for ext in ['', '.mx']:
			path = os.path.join(folder, arg + ext)
			if os.path.exists(path):
				file = Path(path)
	check(isinstance(file, Path), 'File was not found', quoted(arg), insideTestcase=False)
	check(file.suffix == '.mx', "The file is expected to end with '.mx'", quoted(file), insideTestcase=False)
	return file
def modeRun(args):
	if len(args):
		file = processFileArg(args[0])
		print('file:', file)
		assert False, 'Running a single file not implemented yet'
	else:
		runTests('tests')
def modeUpdate(args):
	checkUsage(len(args) >= 1, 'Update specifier expected')
	checkUsage(args[0] == 'output', 'Only', quoted('update output'), 'supported for now', quoted(args[0]))
	args = args[1:]
	if len(args):
		file = processFileArg(args[0])
		updateFile(file)
	else:
		assert False, 'Updating all files outputs not implemented yet'
def test(args):
	if args[0] == 'run':
		modeRun(args[1:])
	elif args[0] == 'update':
		modeUpdate(args[1:])
	else:
		checkUsage(False, "Unknown mode", quoted(args[0]))
# IO ----------------------------------------------
def compileCompiler():
	comm = ['g++', '-std=c++17', 'Masfix.cpp', '-o', 'Masfix']
	print('[CMD]', *comm)
	code, _, _ = runCommand(comm)
	check(code == 0, "g++ error", insideTestcase=False)
def usage():
	print(
"""Usage: test.py <mode>
	modes:
	run                    - test all in the tests folder
	update output <test>   - update the expected output of <test> to the actual output"""
	)
def checkUsage(cond, *messages):
	if not cond:
		print('ERROR:', *messages)
		print()
		usage()
		exit(1)
def getLineArgs() -> list[str]:
	checkUsage(len(sys.argv) >= 2, "Mode expected")
	return sys.argv[1:]
def main():
	args = getLineArgs()
	# compileCompiler() TODO
	test(args)

if __name__ == '__main__':
	main()