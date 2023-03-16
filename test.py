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
def runCommand(command, stdin: str) -> dict:
	stdin = stdin.encode()
	process = Popen(command, stdout=PIPE, stderr=PIPE, stdin=PIPE) # TODO handle process errors
	stdout, stderr = process.communicate(input=stdin)
	stdout = stdout.decode().replace('\r', '') # TODO handle decode errors
	stderr = stderr.decode().replace('\r', '')
	return {'returncode': process.returncode, 'stdout': stdout, 'stderr': stderr}
def parseTestcaseDesc(desc: str):
	expected = {'stdout': '', 'stderr': '', 'stdin': ''}
	while ':' in desc:
		desc = desc[desc.find(':')+1:]
		line = desc.split('\n', maxsplit=1)[0]
		desc = desc[len(line):]
		for fieldType, fieldName in [(int, 'returncode'), (str, 'stdout'), (str, 'stderr'), (str, 'stdin')]:
			if not re.match(f'{fieldName} \d+', line): continue
			num = int(line.split(' ')[1])
			if fieldType == int:
				expected[fieldName] = num
			elif fieldType == str:
				check('\n' in desc, 'Testcase blob must start at a new line')
				s = desc[desc.find('\n')+1:]
				check(len(s) >= num, 'Insufficent character count in description')
				expected[fieldName] = s[:num]
				desc = desc[num:]
			break
		else:
			check(False, 'Unknown field in description', quoted(line))
	check('returncode' in expected, 'Testcase description missing :returncode')
	return expected
def getTestcaseDesc(test: Path, update=False) -> dict:
	path = test.with_suffix('.txt')
	if update:
		if not os.path.exists(path):
			return {'returncode': 0, 'stdout': '', 'stderr': '', 'stdin': ''}
	else:
		check(os.path.exists(path), 'Missing test case desciption', quoted(path))
	with open(path, 'r') as testcase:
		desc = testcase.read()
	return parseTestcaseDesc(desc)
def runFile(prompt, path, stdin) -> dict:
	print(prompt, path)
	return runCommand(['Masfix',  '-s',  '-r', str(path)], stdin)
def runTest(path: Path) -> bool:
	expected = getTestcaseDesc(path)
	ran = runFile('[TESTING]', path, expected['stdin'])
	check(expected['returncode'] == ran['returncode'], 'The return code is not as expected')
	check(expected['stdout'] == ran['stdout'], 'The stdout is not as expected')
	check(expected['stderr'] == ran['stderr'], 'The stderr is not as expected')
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
def saveDesc(file: Path, desc):
	code = desc['returncode']
	stdout = desc['stdout']
	stderr = desc['stderr']
	stdin = desc['stdin']
	with open(file.with_suffix('.txt'), 'w') as f:
		f.write(f':returncode {code}\n\n')
		if stdout: f.write(f':stdout {len(stdout)}\n{stdout}\n\n')
		if stderr: f.write(f':stderr {len(stderr)}\n{stderr}\n\n')
		if stdin: f.write(f':stdin {len(stdin)}\n{stdin}\n\n')
def updateFileOutput(file: Path):
	original = getTestcaseDesc(file)
	ran = runFile('[UPDATING]', file, original['stdin'])
	print('[NOTE] returncode:', ran['returncode'])
	print('[NOTE] stdout:')
	print(ran['stdout'])
	print('[NOTE] stderr:')
	print(ran['stderr'], end='')
	ran['stdin'] = original['stdin']
	saveDesc(file, ran)
def updateInput(file: Path):
	print('[INPUT]', file)
	print('Type your input here, type Ctrl-Z on blank line to end')
	inputed = ''
	try:
		inputed += input()
		while True:
			line = input()
			inputed += '\n' + line
	except EOFError:
		pass
	print('\n[STDIN]', repr(inputed))
	desc = getTestcaseDesc(file, update=True)
	desc['stdin'] = inputed
	saveDesc(file, desc)
	
	print()
	answer = input("Would you like to update output now? (y / n): ")
	if answer and answer[0].lower() == 'y':
		print()
		updateFileOutput(file)

# modes --------------------------------------
def processFileArg(arg) -> Path:
	file = None
	for folder in ['examples', 'tests', '']: # highest priority on the right
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
		print()
		runTests('examples')
def modeUpdate(args):
	checkUsage(len(args) >= 1, 'Update specifier expected')
	update = args[0]
	checkUsage(update in ['output', 'input'], 'Unknown update specifier', quoted(update))
	args = args[1:]
	checkUsage(len(args) >= 1, 'Updated file expected')
	file = processFileArg(args[0])
	if update == 'input':
		updateInput(file)
	elif update == 'output':
		updateFileOutput(file)
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
	run                    - test all in the 'tests', 'examples' folders
	update output <test>   - update the expected output of <test> to the actual output
	update input <test>    - update the stdin passed to <test>"""
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