from subprocess import Popen, PIPE
import os
from pathlib import Path

class TestcaseException(Exception):
	pass

def check(cond, message, insideTestcase=True):
	if not cond:
		print('ERROR:', message)
		if insideTestcase:
			raise TestcaseException(message)
		else:
			exit(1)

def runCommand(command):
	process = Popen(command, stdout=PIPE, stderr=PIPE) # TODO handle process errors
	stdout, stderr = process.communicate()
	stdout = stdout.decode() # TODO handle decode errors
	stderr = stderr.decode()
	return process.returncode, stdout, stderr
def getExpectedOutput(test: Path):
	path = test.with_suffix('.txt')
	check(os.path.exists(path), 'Missing test case desciption: ' + str(path))
	with open(path, 'r') as testcase:
		desc = testcase.read()
	return desc

def compileCompiler():
	comm = ['g++', '-std=c++17', 'Masfix.cpp', '-o', 'Masfix']
	print('[CMD]', *comm)
	code, _, _ = runCommand(comm)
	check(code == 0, "g++ error", False)

def runTest(path: Path) -> bool:
	print('[TESTING]', path)
	check(os.path.exists(path), "Tested file not found")
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

def main():
	compileCompiler()
	runTests('tests')

if __name__ == '__main__':
	main()