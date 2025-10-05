from subprocess import Popen, PIPE
import os, sys
import re
from pathlib import Path

class TestcaseException(Exception):
	pass
class ExecutionException(TestcaseException):
	pass

def quoted(s):
	return "'" + str(s) + "'"
def check(cond, *messages, insideTestcase=True):
	if not cond:
		print('[ERROR]', *messages)
		if insideTestcase:
			raise TestcaseException(*messages)
		else:
			exit(1)
# testcases ------------------------------------
def runCommand(command, stdin: str) -> dict:
	stdin = stdin.encode()
	process = Popen(command, stdout=PIPE, stderr=PIPE, stdin=PIPE) # TODO handle process errors
	try:
		stdout, stderr = process.communicate(input=stdin)
	except (Exception, KeyboardInterrupt) as e:
		print(f'[ERROR] {type(e).__name__}')
		raise ExecutionException(type(e).__name__)
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
			if not re.match(f'{fieldName} \\d+', line): continue
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
	try:
		return parseTestcaseDesc(desc)
	except TestcaseException as e:
		if update:
			print('[NOTE] Using default testcase description\n')
			return {'returncode': 0, 'stdout': '', 'stderr': '', 'stdin': ''}
		raise e
# updates -----------------------------------
def askWhetherToDo(doWhat: str) -> bool:
	answer = input(f'\nWould you like to {doWhat}? (y / n): ')
	print()
	return answer and answer[0].lower() == 'y'
def updateFileOutput(file: Path, verbose=True):
	print('[UPDATING]', file)
	original = getTestcaseDesc(file, update=True)
	ran = runFile(file, original['stdin'])
	if verbose:
		print()
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
	if askWhetherToDo('update output now'):
		updateFileOutput(file)
	
# test ------------------------------------------
def runFile(path, stdin) -> dict:
	return runCommand(['Masfix', '-r', str(path)], stdin)
def runTest(path: Path) -> bool:
	print('[TESTING]', path)
	expected = getTestcaseDesc(path)
	ran = runFile(path, expected['stdin'])
	check(expected['returncode'] == ran['returncode'], 'The return code is not as expected')
	check(expected['stdout'] == ran['stdout'], 'The stdout is not as expected')
	check(expected['stderr'] == ran['stderr'], 'The stderr is not as expected')
	return True
def _handleTestResult(failedTests: list[Path]):
	print()
	if not len(failedTests):
		print('All testcases passed')
	else:
		print('Some testcases failed')
		if askWhetherToDo("update outputs of failed tests"):
			for file in failedTests:
				try:
					updateFileOutput(file, False)
				except ExecutionException:
					print()
		exit(1)
def runTests(dir: Path):
	failedTests = []
	for file in os.listdir(dir):
		path = Path(os.path.join(dir, file))
		if path.is_dir(): path = Path(os.path.join(path, os.path.basename(path.with_suffix('.mx'))))
		if path.suffix != '.mx':
			continue
		try:
			check(os.path.exists(path), 'Testcase not found', quoted(path))
			passed = runTest(path)
		except TestcaseException:
			passed = False
			print()
		if not passed:
			failedTests.append(path)
	_handleTestResult(failedTests)
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

# modes --------------------------------------
def processFileArg(arg) -> Path:
	file = None
	for folder in ['examples', 'tests', '']: # highest priority on the right
		for ext in ['', '.mx']:
			path = os.path.join(folder, arg + ext)
			if os.path.exists(path):
				file = Path(path)
	check(isinstance(file, Path), 'File was not found', quoted(arg), insideTestcase=False)
	if file.is_dir(): file = Path(os.path.join(file, os.path.basename(file.with_suffix('.mx'))))
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
	checkUsage(update in ['output', 'o', 'input', 'i'], 'Unknown update specifier', quoted(update))
	args = args[1:]
	checkUsage(len(args) >= 1, 'Updated file expected')
	file = processFileArg(args[0])
	if update in ['input', 'i']:
		updateInput(file)
	elif update in ['output', 'o']:
		updateFileOutput(file)
def test(args):
	if args[0] in ['run', 'r']:
		modeRun(args[1:])
	elif args[0] in ['update', 'u']:
		modeUpdate(args[1:])
	else:
		checkUsage(False, "Unknown mode", quoted(args[0]))
# IO ----------------------------------------------
def compileCompiler():
	comm = ['g++', 'Masfix.cpp', '-o', 'Masfix']
	print('[CMD]', *comm)
	res = runCommand(comm, '')
	check(res['returncode'] == 0, "g++ error", insideTestcase=False)
def checkSourceCompiled():
	if os.path.getmtime('Masfix.exe') < os.path.getmtime('Masfix.cpp'):
		print("[INFO] 'Masfix.exe' seems older than it's source, recompiling it now.")
		compileCompiler()
		print()
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
	checkSourceCompiled()
	test(args)

if __name__ == '__main__':
	main()
