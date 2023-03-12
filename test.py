from subprocess import Popen, PIPE
import os, sys
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
	stdout = stdout.decode() # TODO handle decode errors
	stderr = stderr.decode()
	return process.returncode, stdout, stderr
def getExpectedOutput(test: Path):
	path = test.with_suffix('.txt')
	check(os.path.exists(path), 'Missing test case desciption', quoted(path))
	with open(path, 'r') as testcase:
		desc = testcase.read()
	return desc
def runTest(path: Path) -> bool:
	print('[TESTING]', path)
	check(os.path.exists(path), "Tested file not found", quoted(path))
	code, stdout, stderr = runCommand(['Masfix',  '-s',  '-r', str(path)])
	expected = getExpectedOutput(path)
	check(expected == stdout, 'The stdout is not as expected')
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
	if success:
		print('\nAll testcases passed')
	else:
		print('\nSome testcases failed')
		exit(1)

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
def checkFileArg(file: Path):
	check(os.path.exists(file) or file is None, "File was not found", quoted(file), insideTestcase=False)
	check(file.suffix == '.mx', "The file is expected to end with '.mx'", quoted(file.suffix), insideTestcase=False)
def processLineArgs() -> dict:
	args = sys.argv[1:]
	checkUsage(len(args) >= 1, "Mode expected")
	flags = {'mode': None, 'second': None, 'file': None}
	if args[0] == 'run':
		flags['mode'] = 'run'
	else:
		checkUsage(False, "Unknown mode", quoted(args[0]))
	assert flags['mode'] == 'run'
	return flags
def main():
	flags = processLineArgs()
	compileCompiler()
	runTests('tests')

if __name__ == '__main__':
	main()