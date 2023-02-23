#include <iostream>
#include <string>
#include <vector>
#include <fstream>
using namespace std;

int main() {
	vector<string> instrs = {"mov 0", "str 1", "mov 1", "str 25"};
	ofstream outFile;
	outFile.open("out.asm");

	for (string s : instrs) {
		outFile << s << '\n';
	}
	outFile.close();
}
