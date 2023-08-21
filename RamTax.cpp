#include <iostream>
#include <vector>
#include <stack>
#include <fstream>

#define BOOT_SIZE 39
#define ASCII_NUMBER_OFFSET 0x30
#define ASCII_CLETTER_OFFSET 0x41
#define ASCII_SLETTER_OFFSET 0x61
#define HEX_LETTER_OFFSET 0xA
using namespace std;

vector<string> types = {"", "dword", "word", "byte", "str", "noth"};
vector<string> spec_words = {"init", "equ", "func", "if", "end.", "cycle"};

#define DWORD 0x01
#define WORD 0x02
#define BYTE 0x03
#define STR 0x04
#define NOTH 0x05

const string ERRORS[] = {
	"GOOD", // 0
#define err_GOOD 0
"UNKNOWN SIGN", // 1
#define err_UNKS 1
"BAD SYMBOL", // 2
#define err_BSYM 2
"HAS ALREADY DECLAIRED", // 3
#define err_HALD 3
"BAD SYNTAX", // 4
#define err_BSTX 4
"BOOT DIRECTIVE TWICE", // 5
#define err_BTTW 5
"UNKNOWN TYPE", // 6
#define err_UNKT 6
"UNKNOWN VARIABLE" // 7
#define err_UNKV 7
, "FILE ERROR" // 8
#define err_FERR 8
, "DECLAIRING FUNCTION INSIDE HOME SECTION" // 9
#define err_DFIH 9
, "DECLAIRING FUNCTION INSIDE ANOTHER FUNCTION" // 10
#define err_DFIF 10
, "UNKNOWN" // 11
#define err_UNKN 11
, "UNKNOWN FUNCTION" // 12
#define err_UNFC 12
, "CANT USE NOTH FUNCTION AS OPERAND" // 13
#define err_CUFO 13
, "TOO MUCH OPERANDS" // 14
#define err_TMOP 14
, "TOO LITTLE OPERANDS" // 15
#define err_TLOP 15
, "USING NOTH FUNCTION AS OPERAND" // 16
#define err_UNOO 16
, "EXPRESSION REQUIRED" // 17
#define err_EXRQ 17
, "BAD VARIABLE NAME" // 18
#define err_BVRN 18
, "CAN'T PUT STRING INTO SHORTER STRING" // 19
#define err_CSIS 19
};


int error = 0;
int error_cause = 0;


const string BOOT[] = {"org 7c00h",
"start:",
"cli" ,
"xor ax, ax",
"mov ds, ax",
"mov es, ax",
"mov ss, ax",
"mov sp, 07c00h",
"sti",

"mov al, 3",
"int 10h",
"mov dh, 0",
"mov dl, 0",
"xor bh, bh",
"int 10h",


"push 0x50",
"pop es",

"mov dx, 0; disk A, head 0 (dl - disk, dh - head)",
"mov ch, 0; 10bit - cyl, 6bit - sector",
"mov cl, 2; sector",
"mov al, 7",
"xor bx, bx",

"mov ah, 02h",
"int 13h",

"mov dl, 0",
"mov dh, 1",
"mov bx, 0x0E00",
"mov ch, 0",
"mov cl, 1",
"mov al, 8", 

"mov ah, 02h",
"int 13h",


"jmp 0080:0000",
"jmp $",

"fd db 'error!'",

"times 510 - ($ - 7c00h) db 0",
"db 055h, 0AAh; ДАЛЬШЕ ЗАГРУЗЧИК НЕ ИДЁТ",
"ORG 500h", 
"jmp HOME" };

vector <string> IN;
vector <string> OUT;
vector <string> FUNC;

vector <pair <string, unsigned char>> DATA;
vector <pair <string, unsigned char>> DATA_FUNC;
vector <pair <string, unsigned char>> DATA_PUBL;

struct STRNS {
	unsigned char type;
	vector<unsigned int> values;
	int length;
};

vector <STRNS> STRS;
vector <pair <string, vector <string> > > FNCS;
stack<pair<int, unsigned char>> cycles;
bool is_boot = 0;
bool is_home = 0;
bool is_func = 0;
int num_cond = 0;
string fnc_name;
string code;
int line = 0;


bool THROW_ERROR(bool Condition, int error_cause, int err = error) {
	if (Condition) {
		error = err;
		std::cerr << code << " : LINE " << line << " : COLUMN " << error_cause << " : " << ERRORS[error];
	}
	return Condition;
}


int strlen(char* _Ch) {
	int i = 0;
	for (; *(_Ch + i) != '\0'; i++) {}
	return i;
}

int CHAR_TO_INT(char* _Ch) {
	int r = 0; // our number that we'll return
	int i = 0; // iterator basically
	int RADIX = 10;
	if (*_Ch == '0' && *(_Ch + 1) == 'x') {
		i += 2; // starting after "0x"
		RADIX += 6; // hexadecimal numbers
	}
	for (; *(_Ch + i) != '\0'; i++) {
		unsigned char t = *(_Ch + i);
		if (t >= '0' && t <= '9')
			r = r * RADIX + (t - ASCII_NUMBER_OFFSET); // adding if 0123456789
		else
		if (t >= 'A' && t <= 'F')
			r = r * RADIX + (t - ASCII_CLETTER_OFFSET + HEX_LETTER_OFFSET); // adding if ABCDEF
		else
		if (t >= 'a' && t <= 'f')
			r = r * RADIX + (t - ASCII_SLETTER_OFFSET + HEX_LETTER_OFFSET); // adding if abcdef
	}
	return r;
}

void stoa(char* a, int num, string& s) {
	for (int i = 0; i < num && i < s.length(); i++) {
		a[i] = s[i];
	}
}

int ContainsThatVar(vector<pair<string, unsigned char>> &dat, std::string &name) {

	for (int i = 0; i < dat.size(); i++) {
		if (dat.at(i).first == name)
			return i;
	}
	return -1;
}

template <class R>

int ContainsThatValue(vector<R>& dat, R& name) {

	for (int i = 0; i < dat.size(); i++) {
		if (dat[i] == name)
			return i;
	}
	return -1;
}

void getToCharFromStr(char* t, string& str, int offset, int howm) {
	int i = 0;
	for (; i < howm && i < str.length(); i++) {
		*(t + i) = str[offset + i];
	}
	*(t + i) = '\0';
}

int STR_TO_INT(string& str) {
	char r[50];
	getToCharFromStr(r, str, 0, str.length());
	return CHAR_TO_INT(r);
}


void WriteString(ofstream& file, string& code) {
	char t[256]; // инициализируем массив символов
	int16_t t_t = code.length() / 256; // сколько раз нужно записать символов по 256
	int8_t rest = code.length() % 256; // оставшиеся
	for (int i = 0; i < t_t; i++) {
		getToCharFromStr(t, code, i * 256, 256); // преобразуем string в char[]
		file.write(t, 256); // записываем
	}
	getToCharFromStr(t, code, t_t * 256, rest); 
	file.write(t, rest); // записываем всё остальное
}

void getToNSpace(int& s, string& str) {
	for (; s < str.length() && (str[s] == 0 || str[s] == ' ' || str[s] == '\t'); s++) {}
}

void getwordUntilSpace(string& name, string& str, int& s) {
	name.clear();
	for (; s < str.length() && !(str[s] == '\0' || str[s] == ' ' || str[s] == '\t'); s++) {
		name.push_back(str.at(s));
	}
}

void getword(string& name, string& str, int& s) {
	name.clear();
	for (; s < str.length() && !(str[s] == '\0' || str[s] == ' ' || str[s] == '\t' || str[s] == '=' || str[s] == '+' || str[s] == '-' || str[s] == '*' || str[s] == '/' || str[s] == '\\' || str[s] == '(' || str[s] == ')' || str[s] == ':' || str[s] == ',' || str[s] == '&' || str[s] == '{' || str[s] == '}' || str[s] == '[' || str[s] == ']'); s++) {
		name.push_back(str.at(s));
	}
}

void getword(string& name, int s, string& str) {
	name.clear();
	for (; !(str[s] == '\0' || str[s] == ' ' || str[s] == '\t' || str[s] == '\n' || str[s] == '=' || str[s] == '+' || str[s] == '-' || str[s] == '*' || str[s] == '/' || str[s] == '\\' || str[s] == '(' || str[s] == ')' || str[s] == ':' || str[s] == ',' || str[s] == '&') || str[s] == '{' || str[s] == '}' || str[s] == '[' || str[s] == ']'; s++) {
		name.push_back(str.at(s));
	}
}

void getToNSpaceIn(int& s, string& cd, int _Howm = 1) {
	s += _Howm;
	getToNSpace(s, cd);
}


vector <pair <string, unsigned char>>& CURRDATA() {
	return is_func ? DATA_FUNC : (is_home? DATA : DATA_PUBL);
}

vector <string>& CURROUT() {
	return is_func ? FUNC : OUT;
}

struct dothing {
	string first, second;
	char sign = '\0';
};

void GivePublics() {
	for (int i = 0; i < DATA_PUBL.size(); i++) {
		CURRDATA().push_back(DATA_PUBL[i]);
	}
}

void defineCondition(string& cond, vector <pair <string, unsigned char>>& DATA, vector<string>& CODE, int& num_con);

bool is_func_returning = true;

unsigned char getSecWord(string& name, int& s, string& str, dothing &Dothing, bool _First, int& num_con) {
	_First ? Dothing.first.clear() : Dothing.second.clear();
	getToNSpace(s, str);

	string signt;
	if (str[s] == '*') {
		signt = "ptr_";
		getToNSpaceIn(s, str);
		if (str[s] == '\n') {
			name = str.substr(s, 3);
		} else
			getword(name, str, s);
		s += 3;
	}
	else {
		getword(name, str, s);
		getToNSpace(s, str);
		for (; s >= str.length(); s--) {}
		if (str.at(s) == '{') {
			int current_func = 0;
			for (; FNCS.at(current_func).first != name; current_func++) {
				if (static_cast<unsigned long long>(current_func) + 1 >= FNCS.size()) {
					return err_UNFC;
				}
			}
			if (FNCS.at(current_func).first.front() == 0xFF && !_First) {
				return err_CUFO;
			}

			is_func_returning = FNCS[current_func].second.front().front() != 0;

			s++;
			getToNSpace(s, str);
			vector<string> values;
			vector<string> e;
			string d;
			for (int brks = 1; brks > 0; s++) {
				if (s >= str.length())
					return err_BSTX;
				if (str[s] == '{')
					brks++;
				else if (str[s] == '}')
					brks--;

				if (str[s] == ',' && brks == 1) {
					s++;
					getToNSpace(s, str);
					values.push_back(d);
					d.clear();
					s--;
				}
				else if (brks != 0) {
					d.push_back(str[s]);
				}
			}

			if (!d.empty())
				values.push_back(d);


			if (values.size() > FNCS[current_func].second.size() - 1) {
				return err_TMOP;
			}

			if (values.size() < FNCS[current_func].second.size() - 1) {
				return err_TLOP;
			}

			for (int g = 0; g < values.size(); g++) {
				defineCondition(values[g], CURRDATA(), CURROUT(), num_con);
				string d = "[fnv_" + name + "_";
				if (g < FNCS[current_func].second.size())
					d += FNCS[current_func].second[g + 1].substr(1);
				else
					return err_TMOP;
				d.push_back(']');
				CURROUT().push_back("push ");
				CURROUT().back() += d;
				e.push_back(d);
				CURROUT().push_back("mov ");
				CURROUT().back() += d;
				CURROUT().back() += ", eax";
			}
			CURROUT().push_back("call fnc_");
			CURROUT().back() += name;
			if (is_func_returning) {
				CURROUT().push_back("mov ");
				CURROUT().back() += _First ? "eax" : "ebx";
				CURROUT().back() += ", [fnv_";
				CURROUT().back() += name;
				CURROUT().back().push_back('_');
				CURROUT().back() += name;
				CURROUT().back().push_back(']');
			}


			for (auto w = e.rbegin(); w != e.rend(); w++) { // string w : e
				CURROUT().push_back("pop ");
				CURROUT().back() += *w;
			}
			signt = "fnc_";

			current_func++;
		}
		else if (name[0] == '\n') {
			signt = "brk_";
			name.erase(0, 1);
		}
		else if (str[s] == '&') {
			signt = "adr_";
			s++;
			getToNSpace(s, str);
			getword(name, str, s);
		}
		else if (name[0] < '0' || name[0] > '9') {
			if (CURRDATA().empty())
				return err_UNKN;
			int i = 0;
			int _str = 0;
			unsigned type_str = 0;
			for (; CURRDATA()[i].first != name; i++) {
				if (i + 1 >= CURRDATA().size()) {
					return err_UNKV;
				}
				if (CURRDATA()[i].second == STR)
					_str++;
			}
			if (CURRDATA()[i].second == DWORD)
				signt = "var_";
			else if (CURRDATA()[i].second == WORD)
				signt = "vrw_";
			else if (CURRDATA()[i].second == BYTE)
				signt = "vrb_";
			if (CURRDATA()[i].second == STR) {
				type_str = STRS[_str].type;
				signt = "str_";
				signt += type_str;
				getToNSpace(s, str);
				if (str[s] != '[')
					return err_BSTX;
				getToNSpaceIn(s, str);
				string cond;
				int brackets = 1;
				for (; s < str.length() && brackets > 0; s++) {
					if (str[s] != ']' || brackets > 1)
						cond.push_back(str[s]);
					if (str[s] == ']')
						brackets--;
					else if (str[s] == '[')
						brackets++;
				}

				defineCondition(cond, CURRDATA(), CURROUT(), num_con);

				if (error) {
					error_cause = s;
					return error;
				}

				if (type_str == WORD) {
					CURROUT().push_back("shl eax, 1");
				}
				else if (type_str == DWORD) {
					CURROUT().push_back("shl eax, 2");
				}

				CURROUT().push_back("add eax, var_");
				CURROUT().back() += name;
				CURROUT().push_back("push eax");

				s++; // not to count ] as sign in future
			}
		}
	}
	

	_First ? Dothing.first = signt + name : Dothing.second = signt + name;

	return 0;
}

void defineCondition(string& cond, vector <pair <string, unsigned char>>& DATA, vector<string>& CODE, int& num_con) {
	string varname;
	vector <dothing> d;
	dothing Dothing;
	int this_sym = 0;
	int brackets = 0;
	int br;
	char t[4];
	t[0] = '\n';
	t[3] = '\0';
	int i = 0;
	while (!(cond.length() == 3 && cond.front() == '\n')) {
		Dothing.sign = '\0';
		Dothing.first.clear();
		Dothing.second.clear();
		for (i = 0; i < cond.length() && cond[i] != ')'; i++) {
			if (cond[i] == '(') {
				this_sym = i;
			}
		}
		br = this_sym;
		if (cond[this_sym] == '(')
			this_sym++;
		unsigned char wrd = getSecWord(varname, this_sym, cond, Dothing, 1, num_con);
		if (wrd != 0) {
			error_cause = this_sym;
			error = wrd;
			return;
		}
		getToNSpace(this_sym, cond);
		if (this_sym + 1 < cond.length()) {
			if (cond.substr(this_sym, 2) == "/=") {
				Dothing.sign = '!';
				this_sym++;
			}
			else if (cond.substr(this_sym, 2) == "<<") {
				Dothing.sign = '\x1';
				this_sym++;
			}
			else if (cond.substr(this_sym, 2) == ">>") {
				Dothing.sign = '\x2';
				this_sym++;
			} else
				Dothing.sign = cond.at(this_sym);
		}
		else
			Dothing.sign = '\0';


		if (Dothing.sign != ')' && this_sym + 1 < cond.length()) {
			this_sym++;
			unsigned char wrd = getSecWord(varname, this_sym, cond, Dothing, 0, num_con);
			if (wrd != 0) {
				error_cause = this_sym;
				error = wrd;
				return;
			}
		}
		getToNSpaceIn(this_sym, cond);
		if (this_sym < cond.length()) {
			this_sym--;
			if (cond.at(this_sym) == ')')
				br++;
		}

		cond.erase(br, this_sym - br);
		int lenbef = cond.length();
		if (!((Dothing.first[0] >= '0' && Dothing.first[0] <= '9') && (Dothing.second[0] >= '0' && Dothing.second[0] <= '9'))) {
			d.push_back(Dothing);
			t[1] = char(brackets / 10 + 0x30);
			t[2] = char(brackets % 10 + 0x30);
			cond.insert(br, t);
			brackets++;
		}
		else {
			string our_new_into;
			char into[65], into2[65];
			unsigned long into3;
			int len = Dothing.first.length();
			getToCharFromStr(into, Dothing.first, len < 64 ? 0 : len - 64, len < 64 ? len : 64);
			len = Dothing.second.length();
			getToCharFromStr(into2, Dothing.second, len < 64 ? 0 : len - 64, len < 64 ? len : 64);

			int io = CHAR_TO_INT(into);
			int io2 = CHAR_TO_INT(into2);
			switch (Dothing.sign)
			{
			case '\0':
				into3 = io;
			case '+':
				into3 = io + io2;
				break;
			case '-':
				into3 = io - io2;
				break;
			case '*':
				into3 = io * io2;
				break;
			case '/':
				into3 = io / io2;
				break;
			case '%':
				into3 = io % io2;
				break;
			case '>':
				into3 = io > io2;
				break;
			case '<':
				into3 = io < io2;
				break;
			case '=':
				into3 = io == io2;
				break;
			case '!':
				into3 = io != io2;
				break;
			case '\x2':
				into3 = io >> io2;
				break;
			case '\x1':
				into3 = io << io2;
				break;
			default:
				error = err_UNKS;
				break;
			}
			_itoa_s(into3, into, 16);
			our_new_into = "0x";
			our_new_into += into;
			cond.insert(br, our_new_into);
		}

		if (this_sym < lenbef)
			this_sym = br;
		else this_sym = 0;
	}

	const char CMPEAXEBX[] = "cmp eax, ebx";

	for (i = 0; i < d.size(); i++) {

		if (d[i].first.empty()) {
			error = err_EXRQ;
			return;
		}
		unsigned char type1, type2;

		if (!d[i].second.empty()) {
			if (is_func_returning) {
				if (d[i].second.substr(0, 3) != "brk") {
					if (d[i].second.substr(0, 3) != "fnc") {
						bool ptre = d[i].second.substr(0, 3) == "ptr";
						bool stre = d[i].second.substr(0, 3) == "str";

						if (stre)
							CODE.push_back("pop esi");

						if (!(ptre && d[i].first[4] == '\n')) {
							if (d[i].second.substr(0, 3) == "var" || (stre && d[i].second[4] == DWORD)) {
								CODE.push_back("mov ebx, ");
								type2 = DWORD;
							}
							else if (d[i].second.substr(0, 2) == "vr" || (stre && d[i].second[4] == WORD) || (stre && d[i].second[4] == BYTE)) {
								CODE.push_back("xor ebx, ebx");
								CODE.push_back("mov b");
								if (d[i].second[2] == 'w' || (stre && d[i].second[4] == WORD)) { // word?
									CODE.back() += 'x'; // bx
									type2 = WORD;
								}
								else if (d[i].second[2] == 'b' || (stre && d[i].second[4] == BYTE)) { // byte?
									CODE.back() += 'l'; // bl
									type2 = BYTE;
								}
								CODE.back() += ", ";
							}
							else {
								char numero[64];
								getToCharFromStr(numero, d[i].second, 0, d[i].second.length());
								int into3 = CHAR_TO_INT(numero);

								if (into3 <= INT8_MAX * 2 + 1) {
									CODE.push_back("xor ebx, ebx");
									CODE.push_back("mov bl, ");
									type2 = BYTE;
								}
								else if (into3 <= INT16_MAX * 2 + 1) {
									CODE.push_back("xor eax, eax");
									CODE.push_back("mov bx, ");
									type2 = WORD;
								}
								else if (into3 <= unsigned long(INT32_MAX) * 2 + 1) {
									CODE.push_back("mov ebx, ");
									type2 = DWORD;
								}

							}
							bool e = d[i].second.substr(0, 3) == "adr";
							if (d[i].second.front() == 'v' || e || ptre || stre) {
								if (!e)
									CODE.back() += '[';
								if (!stre) {
									if (!is_func)
										if (e || ptre || type2 == WORD || type2 == BYTE) {
											CODE.back() += "var_";
											CODE.back() += d[i].second.substr(4);
										}
										else CODE.back() += d[i].second;
									else {
										CODE.back() += "fnv_";
										CODE.back() += fnc_name;
										CODE.back() += '_';
										CODE.back() += d[i].second.substr(4);

									}
								}
								else
									CODE.back() += "si";

								if (!e)
									CODE.back() += ']';
								if (ptre) {
									CODE.push_back("mov di, bx");
									CURROUT().push_back("xor bx, bx");
									CURROUT().push_back("shr ebx, 4");
									CURROUT().push_back("mov es, bx");
									CURROUT().push_back("mov ebx, [es:di]");
								}
							}
							else if (STR_TO_INT(d[i].second) != 0) { // если не ноль
								CODE.back() = ("mov ebx, ");
								CODE.back() += d[i].second;
							}
							else if (type2 != DWORD)
								CURROUT().pop_back();
						}
						
					}
				}
				else {
					error = err_UNOO;
					return;
				}

			}
			else if (CODE.back() != "push ebx")
				CODE.push_back("pop ebx");
			else
				CODE.pop_back();
		}

		if (is_func_returning) {
			if (d[i].first.substr(0, 3) != "brk") {
				if (d[i].first.substr(0, 3) != "fnc") {
					bool ptre = d[i].first.substr(0, 3) == "ptr";
					bool stre = d[i].first.substr(0, 3) == "str";
					if (stre)
						CODE.push_back("pop esi");
					if (!(ptre && d[i].first[4] == '\n')) {
						if (d[i].first.substr(0, 3) == "var" || (stre && d[i].first[4] == DWORD)) {
							CODE.push_back("mov eax, ");
							type1 = DWORD;
						}
						else if (d[i].first.substr(0, 2) == "vr" || (stre && d[i].first[4] == WORD) || (stre && d[i].first[4] == BYTE)) {
							CODE.push_back("xor eax, eax");
							CODE.push_back("mov a");
							if (d[i].first[2] == 'w' || (stre && d[i].first[4] == WORD)) {
								CODE.back() += 'x'; // ax
								type1 = WORD;
							}
							else if (d[i].first[2] == 'b' || (stre && d[i].first[4] == BYTE)) {
								CODE.back() += 'l'; // al
								type1 = BYTE;
							}
							CODE.back() += ", ";

						}
						else {
							char e[64];
							getToCharFromStr(e, d[i].first, 0, d[i].first.length());
							int into3 = CHAR_TO_INT(e);


							if (into3 <= INT8_MAX * 2 + 1) {
								CODE.push_back("xor eax, eax");
								CODE.push_back("mov al, ");
								type1 = BYTE;
							}
							else if (into3 <= INT16_MAX * 2 + 1) {
								CODE.push_back("xor eax, eax");
								CODE.push_back("mov ax, ");
								type1 = WORD;
							}
							else if (into3 <= unsigned long(INT32_MAX) * 2 + 1) {
								CODE.push_back("mov eax, ");
								type1 = DWORD;
							}

						}

						bool e = d[i].first.substr(0, 3) == "adr";

						if (d[i].first.front() == 'v' || e || ptre || stre || d[i].first.substr(0, 2) == "vr") {

							if (!e)
								CODE.back() += '[';

							if (!stre) {
								if (!is_func) {
									if (e || ptre || type1 == WORD || type1 == BYTE) {
										CODE.back() += "var_";
										CODE.back() += d[i].first.substr(4);
									}
									else CODE.back() += d[i].first;
								}
								else {
									CODE.back() += "fnv_";
									CODE.back() += fnc_name;
									CODE.back() += '_';
									CODE.back() += d[i].first.substr(4);
								}
							}
							else
								CODE.back() += "si";

							if (!e)
								CODE.back() += ']';
							if (ptre) {
								CURROUT().push_back("mov di, ax");
								CURROUT().push_back("xor ax, ax");
								CURROUT().push_back("shr eax, 4");
								CURROUT().push_back("push es, ax");
								CURROUT().push_back("mov eax, [es:di]");
							}
						}
						else if (STR_TO_INT(d[i].first) != 0) { // если не ноль
							CURROUT().pop_back();
							CODE.back() = ("mov eax, ");
							CODE.back() += d[i].first;
						}
						else if (type1 != DWORD)
							CURROUT().pop_back();
					}
					else if (CODE.back() != "push eax")
						CODE.push_back("pop eax");
				}
			}
			else if (CODE.back() != "push eax")
				CODE.push_back("pop eax");
			else
				CODE.pop_back();
		}

		char r[30];
		char r2[30];
		_itoa_s(num_con, r, 10);
		_itoa_s(i, r2, 10);
		//string AREG, BREG;
		switch (d[i].sign)
		{
		case '\0':
			break;
		case '+':
			CODE.push_back("add eax, ebx");
			break;
		case '-':
			CODE.push_back("sub eax, ebx");
			break;
		case '*':
			CODE.push_back("mul ebx");
			break;
		case '/':
			CODE.push_back("xor edx, edx");
			CODE.push_back("div ebx");
			break;
		case '%':
			CODE.push_back("xor edx, edx");
			CODE.push_back("div ebx");
			CODE.push_back("mov eax, edx");
			break;
		case '>':
			CODE.push_back("xor edx, edx");
			CODE.push_back(CMPEAXEBX);
			CODE.push_back("mov eax, edx");
			CODE.push_back("jnb cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.push_back("inc al");
			CODE.push_back("cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.back() += ':';
			break;
		case '<':
			CODE.push_back("xor edx, edx");
			CODE.push_back(CMPEAXEBX);
			CODE.push_back("mov eax, edx");
			CODE.push_back("jnl cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.push_back("inc al");
			CODE.push_back("cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.back() += ':';
			break;
		case '=':
			CODE.push_back("xor edx, edx");
			CODE.push_back(CMPEAXEBX);
			CODE.push_back("mov eax, edx");
			CODE.push_back("jne cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.push_back("inc al");
			CODE.push_back("cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.back() += ':';
			break;
		case '!':
			CODE.push_back("xor edx, edx");
			CODE.push_back(CMPEAXEBX);
			CODE.push_back("mov eax, edx");
			CODE.push_back("je cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.push_back("inc al");
			CODE.push_back("cnd_");
			CODE.back() += r;
			CODE.back() += '_';
			CODE.back() += r2;
			CODE.back() += ':';
			break;
		case '\x1':
			CODE.push_back("mov dl, cl");
			CODE.push_back("mov cl, bl");
			CODE.push_back("shl eax, cl");
			break;
		case '\x2':
			CODE.push_back("mov dl, cl");
			CODE.push_back("mov cl, bl");
			CODE.push_back("shr eax, cl");
			break;
		default:
			error = err_UNKS;
			break;
		}
		if (i + 1 < d.size())
			CODE.push_back("push eax");

		/*          CODE.push_back("push eax");
			  CODE.push_back("mov eax, ");
			  if (into3 <= BYTE_MAX * 2 + 1)
				  CODE.back() += "byte";
			  else if (into3 <= INT16_MAX * 2 + 1)
				  CODE.back() += "word";
			  else if (into3 <= unsigned long (INT32_MAX) * 2 + 1)
				  CODE.back() += "dword";*/
	}
		num_con++;
		cond.clear();
}
	

	//for (i = 0; i < cond.length() && ((cond[i] >= 'A' && cond[i] <= 'Z') || (cond[i] >= 'a' && cond[i] <= 'z') || (cond[i] == '_')); i++) {
	   // может ли это быть названием переменной?
	//        varname.push_back(cond[i]);
	//}
	//int j;
	//for (j = 0; j < DATA.size() && DATA.at(j).first != varname; j++) {}
	//if (DATA[j].first != varname) {
	//    dothing.first.first = "var_" + varname;
	//}
	//else {
	//    !error;
	//    return;
	//}





void g(vector <pair <string, unsigned char>>& DATA, string& code, bool is_boot, vector <string>& OUT, vector <STRNS>& STRS, int MEANS, string signt) {
	int strc = 0;
	char type[] = " db ";

	for (int i = 0; i < DATA.size(); i++) {
		switch (DATA[i].second)
		{
		case DWORD: //DWORD
			code = signt + '_' + DATA[i].first + " dd 0";
			OUT.insert(OUT.begin() + is_boot * BOOT_SIZE + MEANS, code);
			break;
		case WORD:
			code = signt + '_' + DATA[i].first + " dw 0";
			OUT.insert(OUT.begin() + is_boot * BOOT_SIZE + MEANS, code);
			break;
		case BYTE:
			code = signt + '_' + DATA[i].first + " db 0";
			OUT.insert(OUT.begin() + is_boot * BOOT_SIZE + MEANS, code);
			break;
		case STR: 
			switch (STRS[strc].type)
			{
			case BYTE:
				type[2] = 'b';
				break;
			case WORD:
				type[2] = 'w';
				break;
			case DWORD:
				type[2] = 'd';
				break;
			default:
				break;
			}
			char e[30];
			code.clear();
			if (STRS[strc].length > STRS[strc].values.size()) {
				_itoa_s(STRS[strc].length - STRS[strc].values.size(), e, 10);
				code = "times ";
				code += e;
				code += type;
				code += "0";
			}
			
			char s[11];
			if (!STRS[strc].values.empty()) {
				code = '\n' + code;
				for (auto we = STRS[strc].values.rbegin(); we != STRS[strc].values.rend(); we++) {
					_itoa_s(*we, s, 10);
					if (we != STRS[strc].values.rbegin())
						code = ", " + code;
					code.insert(0, s);
				}

				code = type + code; // db
			}
			code.insert(0, ":\n");
			code = DATA[i].first + code; // var_a
			code = "var_" + code;
			OUT.insert(OUT.begin() + is_boot * BOOT_SIZE + MEANS, code);
			strc++;
			break;
		default:
			break;
		}
	}
}




int main(int argc, char* argv[])
{
	string in, out;
	if (argc > 1)
		in = argv[1];
	else
		in = "file.rmt";
	if (argc > 2)
		string out = argv[2];
	else
		out = "out.asm";
	
	string str;
	string cond;
	string name;
	char r[65];
	ifstream iffile(in);
	if (!iffile.is_open()) {
		std::cout << "infile error!";
		return -1;
	}
	ofstream offile(out, ios::app);

	if (!offile.is_open()) {
		std::cout << "offile error!";
		return -2;
	}
	
	offile.close();

	while (!iffile.eof()) {
		char f = 0;
		int e = 0;
		code.clear();
		bool comment = 0;
		while (f != '\n' && !iffile.eof()) {
			f = iffile.get();
			if (f == '#' && !comment)
				comment = 1;
			if (!comment && f != '\n' && !iffile.eof())
				code.push_back(f);
		}
		getToNSpace(e, code);
		if (!code.substr(e).empty())
			IN.push_back(code);
	}

	iffile.close();

	for (line = 0; line < IN.size(); line++) {
		code = IN[line];
		std::cout << code << endl;

		int offset_from_space = 0;
		int row_start = 0;
		getToNSpace(row_start, code); //подобраться к не-пробелу
		CURROUT().push_back("; ####### ");
		CURROUT().back() += code.substr(row_start);
		if (!code.substr(row_start).empty())
		if (code.substr(row_start, 5) == "init ") { // инициализация переменных
			row_start += 5;
			unsigned char type = 0;
			unsigned char type_str = 0;
			unsigned int length = 0;
			getToNSpace(row_start, code);
			getword(name, code, row_start);
			for (type = 0; name != types[type]; type++) {
				if (!(type < types.size())) {
					error = 6;
					std::cerr << code << " : " << line << " : " << ERRORS[error];
					return -1;
				}
			}

			row_start++;

			if (type == STR) { // extra information about str var

				offset_from_space = row_start;
				getword(name, code, row_start);
				if (name[0] >= '0' && name[0] <= '9') { // if there's number of chars
					length = STR_TO_INT(name);
					getToNSpaceIn(row_start, code);
					getword(name, code, row_start);
				}
					
				for (type_str = 0; name != types[type_str]; type_str++) { // type of str
					if (THROW_ERROR(!(type_str < types.size()), row_start, err_UNKT)) return -1;
				}
				row_start++;
			}

			
			getToNSpace(row_start, code);
			getwordUntilSpace(name, code, row_start);
			for (int i = 0; i < name.length(); i++) {
				if (THROW_ERROR(!((name.at(i) >= 'A' && name.at(i) <= 'Z') ||
					(name.at(i) >= 'a' && name.at(i) <= 'z') ||
					(name.at(i) == '_')), row_start, err_BVRN)) return -1;
			}
			int a;
			if (!is_func)
				a = ContainsThatVar(DATA, name);
			else
				a = ContainsThatVar(DATA_FUNC, name);
			if (a == -1) {
				CURRDATA().push_back(pair(name, type));
			}
			else if (THROW_ERROR(CURRDATA().at(a).second != type, row_start - name.length(), err_HALD)) 
				return -1;
		 
			getToNSpaceIn(row_start, code);
			
			if (row_start < code.length()) {
				if (code[row_start] == '=') {
					getToNSpaceIn(row_start, code);
					
					if (THROW_ERROR(row_start >= code.length(), row_start, err_TLOP))
						return -1;
				}
				else if (THROW_ERROR(length == 0, offset_from_space, err_TLOP))
						return -1;
			}
			
			
			if (type == STR) {
				vector<unsigned int> symbArray; // массив символов для определения строки
				
				if (row_start < code.length()) {
					if (THROW_ERROR(code[row_start] != '\"', row_start, err_BSTX))
						return -1;
					row_start++;

					for (; code[row_start] != '"'; row_start++) {
						if (THROW_ERROR(row_start >= code.length() - 1, row_start, err_TLOP))
							return -1;

						if (!symbArray.empty()) {
							if (symbArray.back() == '\\') {
								char t[5] = "0x\0\0";
								unsigned char e;
								switch (code[row_start])
								{
								case '0':
									symbArray.back() = ('\0');
									break;
								case 'x':
									for (unsigned char i = 0; i < 2 && row_start + 1 < code.length(); i++) {
										if ((code[row_start + 2] >= '0' && code[row_start + 1] <= '9') || (code[row_start + 2] >= 'a' && code[row_start + 1] <= 'f') || (code[row_start + 2] >= 'A' && code[row_start + 1] <= 'F')) {
											t[2 + i] = code[row_start + 1];
											row_start++;
										}
									}
									e = CHAR_TO_INT(t);
									symbArray.back() = (e);
									break;
								case 'a':
									symbArray.back() = ('\a');
									break;
								case 'b':
									symbArray.back() = ('\b');
									break;
								case 't':
									symbArray.back() = ('\t');
									break;
								case 'n':
									symbArray.back() = ('\n');
									break;
								case 'v':
									symbArray.back() = ('\v');
									break;
								case 'f':
									symbArray.back() = ('\f');
									break;
								case 'r':
									symbArray.back() = ('\r');
									break;
								case '"':
									symbArray.back() = ('\"');
									break;
								case '\\':
									symbArray.back() = ('\\');
									break;
								default:
									if (THROW_ERROR(true, row_start, err_BSTX))
										return -1;
									break;
								}
							}
							else
								symbArray.push_back(code[row_start]);
						}
						else
							symbArray.push_back(code[row_start]);
					}
					
				}
				if (length == 0)
					length = symbArray.size();
				else if (THROW_ERROR(length < symbArray.size(), 
					offset_from_space, err_CSIS))
						return -1;

				STRS.push_back(STRNS(type_str, symbArray, length));
			}
		}
		else if (code.substr(row_start, 3) == "if ") {
			row_start += 3;
			int row_end;
			cond.clear();
			for (row_end = row_start; code.substr(row_end, 4) != " do:"; row_end++) {
				cond.push_back(code.at(row_end));
			}
			defineCondition(cond, CURRDATA(), CURROUT(), num_cond);
			
			if (THROW_ERROR(error, error_cause))
				return -1;

			CURROUT().push_back("test eax, eax");
			CURROUT().push_back("jz if_");
			char r[65];
			_itoa_s(num_cond, r, 10);
			OUT.back() += r;
			cycles.push(pair(num_cond, 0));
		}
		else if (code.substr(row_start, 3) == "end") {
			switch (cycles.top().second)
			{
			case 0x00: // if
				CURROUT().push_back("if_");
				break;
			case 0x01: // inft
				CURROUT().push_back("ifn_");
				break;
			case 0x80: // cycle
				CURROUT().push_back("loop ccl_");
				break;
			case 0x81: // func
				CURROUT().push_back("ret");

				if (FNCS.back().second[0][0] == NOTH) { // не является ли функция возвращающей
					DATA_FUNC.erase(DATA_FUNC.begin());
				}

				name = "fnv_" + fnc_name;
				g(DATA_FUNC, code, false, FUNC, STRS, 0, name);
				for (string i : FUNC) {
					OUT.push_back(i);
				}

				is_func = 0;
				break;
			case 0x02: // until
				CURROUT().push_back("jmp til_");
				break;
			default:
				break;
			}
			char r[65];
			_itoa_s(cycles.top().first, r, 10);
			if (cycles.top().second != 0x81)
				CURROUT().back() += r;
			if (cycles.top().second == 0x02) {
				CURROUT().push_back("tle_");
				CURROUT().back() += r;
			}
			if (cycles.top().second < 0x80)
				CURROUT().back() += ":";
			else if (cycles.top().second != 0x81)
				CURROUT().push_back("pop ecx");
			cycles.pop();

		}
		else if (code.substr(row_start, 4) == "equ " || code.substr(row_start, 4) == "inc " || code.substr(row_start, 4) == "dec ") {
			
			unsigned char special_cmd = 0;
			if (code.substr(row_start, 4) == "inc ")
				special_cmd = 1;
			else if (code.substr(row_start, 4) == "dec ")
				special_cmd = 2;
			bool ptr7 = false;
			getToNSpaceIn(row_start, code, 4);
			if (code[row_start] == '*') {
				ptr7 = true;
				getToNSpaceIn(row_start, code);
			}
			getword(name, code, row_start);

			if (THROW_ERROR((DATA.empty() && !is_func), row_start, err_UNKV))
				return -1;

			int i = 0;
			unsigned int str_it = 0;
			for (; CURRDATA().at(i).first != name; i++) {
				if (THROW_ERROR(i >= (CURRDATA().size()) - 1, row_start, err_UNKV))
					return -1;

				if (CURRDATA().at(i).second == STR)
					str_it++;
			}
			bool str_eto = false;
			unsigned char type_str = 0;
			if (CURRDATA().at(i).second == STR) {
				type_str = STRS[str_it].type;
				str_eto = true;
				getToNSpace(row_start, code);

				if (THROW_ERROR(code[row_start] != '[', row_start, err_BSTX))
					return -1;

				getToNSpaceIn(row_start, code);
				cond.clear();
				int brackets = 1;
				for (; row_start < code.length() && brackets > 0; row_start++) {
					if (code[row_start] != ']' || brackets > 1)
						cond.push_back(code[row_start]);
					if (code[row_start] == ']')
						brackets--;
					else if (code[row_start] == '[')
						brackets++;
				}
				defineCondition(cond, CURRDATA(), CURROUT(), num_cond);

				if (THROW_ERROR(error, error_cause))
					return 227;

				/*CURROUT().push_back("mov esi, var_");
				CURROUT().back() += name;
				CURROUT().push_back("add esi, eax");
				CURROUT().push_back("shl esi, 12");
				CURROUT().push_back("shr si, 12");
				CURROUT().push_back("mov bx, si");
				CURROUT().push_back("shr si, 16");
				CURROUT().push_back("mov ds, si");
				CURROUT().push_back("mov si, bx");
				CURROUT().push_back("mov [es:di]");*/
				CURROUT().push_back("mov si, var_");
				CURROUT().back() += name;
				if (type_str == WORD) {
					CURROUT().push_back("shl eax, 1");
				} else if (type_str == DWORD) {
					CURROUT().push_back("shl eax, 2");
				}
				CURROUT().push_back("add esi, eax");
				name = "si";
			}

			if (!special_cmd) {
				getToNSpaceIn(row_start, code);

				if (THROW_ERROR(code.at(row_start) != '=', row_start, err_BSTX))
					return -1;

				getToNSpaceIn(row_start, code);
				int offset = row_start;
				cond.clear();
				for (; row_start < code.length(); row_start++) {
					cond.push_back(code.at(row_start));
				}
				defineCondition(cond, CURRDATA(), CURROUT(), num_cond);

				if (THROW_ERROR(error, error_cause + offset))
					return -1;

				CURROUT().push_back("mov ");
			}
			else if (special_cmd == 1) {
				CURROUT().push_back("inc ");
			}
			else if (special_cmd == 2) {
				CURROUT().push_back("dec ");
			}
			
				//OUT.push_back("pushad");
				if (!is_func) {
					if (ptr7) {
						CURROUT().pop_back();
						CURROUT().push_back("mov edx, ");
					}
					CURROUT().back() += '[';
					if (!str_eto)
						CURROUT().back() += "var_";
				}
				else {
					CURROUT().back() += '[';
					if (!str_eto) {
						CURROUT().back() += "fnv_";
						CURROUT().back() += fnc_name;
						CURROUT().back() += '_';
					}
				}
				CURROUT().back() += name;
				CURROUT().back() += ']';
				if (ptr7) {
					CURROUT().push_back("mov di, dx");
					CURROUT().push_back("xor dx, dx");
					CURROUT().push_back("shr edx, 4");
					CURROUT().push_back("mov es, dx");
					if (!special_cmd)
						CURROUT().push_back("mov");
					else if (special_cmd == 1) {
						CURROUT().push_back("inc");
					}
					else if (special_cmd == 2) {
						CURROUT().push_back("dec");
					}
					CURROUT().back() += (" [es:di]");
				}
				if (!special_cmd) // lalala
				if (CURRDATA().at(i).second == DWORD || type_str == DWORD) {
					CURROUT().back() += ", eax";
				}
				else if (CURRDATA().at(i).second == WORD || type_str == WORD) {
					CURROUT().back() += ", ax";
				}
				else if (CURRDATA().at(i).second == BYTE || type_str == BYTE) {
					CURROUT().back() += ", al";
				}

				//OUT.push_back("popad");

		}

		else if (code.substr(row_start, 5) == "ifnt ") {
			CURROUT().push_back("jmp ifn_");
			_itoa_s(cycles.top().first, r, 10);
			CURROUT().back() += r;
			CURROUT().push_back("if_");
			CURROUT().back() += r;
			CURROUT().back() += ':';
			cycles.top().second = 1;
		}
		else if (code.substr(row_start, 5) == "HOME:") {
			is_home = true;
			GivePublics();
			OUT.push_back("HOME:");
		}
		else if (code.substr(row_start, 5) == "HEND.") {
			CURROUT().push_back("hlt");
			CURROUT().push_back("HEND:");
		}
		else if (code[row_start] == '$') {
			getToNSpaceIn(row_start, code);
			if (code.substr(row_start, 8) == "BOOT") {
				if (!is_boot) {
					for (string f : BOOT) {
						CURROUT().push_back(f);
					}
					is_boot = true;
				}
				else {
					THROW_ERROR(true, row_start, err_BTTW);
					return -1;
				}
			}
			else if (code.substr(row_start, 4) == "MEAN") {
				string to_get_replaced, to_replace_with;
				getToNSpaceIn(row_start, code, 5);
				getword(to_get_replaced, code, row_start);

				for (int iw = 0; iw < to_get_replaced.length(); iw++) {
					if (THROW_ERROR(!((to_get_replaced.at(iw) >= 'A' && to_get_replaced.at(iw) <= 'Z') ||
					(to_get_replaced.at(iw) >= 'a' && to_get_replaced.at(iw) <= 'z') ||
					(to_get_replaced.at(iw) == '_')), iw, err_BVRN))
						return -1;// может ли это быть названием переменной?
				}

				getToNSpaceIn(row_start, code);
				for (; row_start < code.length(); row_start++) {
					to_replace_with.push_back(code[row_start]);
				}
				int diff = to_get_replaced.length() - to_replace_with.length();
				IN.erase(IN.begin() + line, IN.begin() + line + 1);
				for (int ie = line; ie < IN.size(); ie++) {
					row_start = 0;
					while (row_start < IN[ie].size())//row start
					{
						if ((IN[ie][row_start] >= 'A' && IN[ie][row_start] <= 'Z') ||
							(IN[ie][row_start] >= 'a' && IN[ie][row_start] <= 'z') ||
							(IN[ie][row_start] == '_')) {
							getword(name, IN[ie], row_start);
							if (name == to_get_replaced) {
								IN[ie].erase(IN[ie].begin() + row_start - to_get_replaced.length(), IN[ie].begin() + row_start);
								if (row_start > IN[ie].length())
									row_start = IN[ie].length();
								IN[ie].insert(row_start - to_get_replaced.length(), to_replace_with);
								row_start += to_replace_with.length();
							}
						}
						else
							row_start++;
						
						getToNSpace(row_start, IN[ie]);
					}
				}
				line--;
			}
			else if (code.substr(row_start, 3) == "GET") {
				IN.erase(IN.begin() + line);
				name.clear();
				for (row_start += 4; row_start < code.length(); row_start++) {
					name.push_back(code[row_start]);
				}
				ifstream GET; // достаём требующийся файл
				GET.open(name);

				if (THROW_ERROR(!GET.is_open(), row_start - name.length(), err_FERR))
					return -1;


				for (int lineGET = line; !GET.eof(); lineGET++) { // 
					char f = 0;
					code.clear();
					while (f != '\n' && !GET.eof()) {
						f = GET.get();
						if (f != '\n' && !GET.eof())
							code.push_back(f);
					}
					IN.insert(IN.begin() + lineGET, code);
				}
				GET.close();
				line--;
			}
		}
		else if (code.substr(row_start, 4) == "ASM:") {
			int row = 0;
			while (code.substr(row, 5) != "AEND.") {
				if (line + 1 < IN.size())
					code = IN[++line];
				else {
					error = err_BSTX;
					std::cerr << code << " : " << line << " : " << ERRORS[error];
					return error;
				}
				std::cout << code << endl;
				row = 0;
				getToNSpace(row, code);
				if (code.substr(row, 5) != "AEND.")
					is_func ? FUNC.push_back(code.substr(row)) : OUT.push_back(code.substr(row));
			}

		}
		else if (code.substr(row_start, 5) == "func ") {
			if (is_home) {
				error = err_DFIH;
				std::cerr << code << " : " << line << " : " << ERRORS[error];
				return error;
			}
			if (is_func) {
				error = err_DFIF;
				std::cerr << code << " : " << line << " : " << ERRORS[error];
				return error;
			}

			is_func = true;
			DATA_FUNC.clear();
			FUNC.clear();
			row_start += 5;
			getToNSpace(row_start, code);

			getword(name, code, row_start);

			int u = ContainsThatValue(types, name);
			if (u != -1) {
				row_start++;
				getword(name, code, row_start);
				for (int i = 0; i < FNCS.size(); i++) {
					if (FNCS.at(i).first == name) {
						error = err_HALD;
						std::cerr << code << " : " << line << " : " << ERRORS[error];
						return error;
					} 
				}
				DATA_FUNC.push_back(pair(name, unsigned char(u)));
				fnc_name = name;
			}
			else {
				error = 6;
				std::cerr << code << " : " << line << " : " << ERRORS[error];
				return error;
			}
			getToNSpace(row_start, code);
			if (code[row_start] != ':') {
				error = err_BSTX;
				std::cerr << code << " : " << line << " : " << ERRORS[error];
				return error;
			}
			row_start++;
			getToNSpace(row_start, code);

			while (code.substr(row_start, 3) != "do:") {
				if (row_start >= code.length()) {
					error = err_BSTX;
					std::cerr << code << " : " << line << " : " << ERRORS[error];
					return error;
				}
				getword(name, code, row_start);
				int u = ContainsThatValue(types, name);
				if (u != -1) {
					row_start++;
					getword(name, code, row_start);
					if (ContainsThatVar(DATA_FUNC, name) == -1) {
						DATA_FUNC.push_back(pair(name, unsigned char(u)));
					}
					else {
						error = err_HALD;
						std::cerr << code << " : " << line << " : " << ERRORS[error];
						return error;
					}
				}
				else {
					error = err_UNKT;
					std::cerr << code << " : " << line << " : " << ERRORS[error];
					return error;
				}
				getToNSpace(row_start, code);
				if (code.substr(row_start, 3) != "do:") {
					if (code[row_start] != ',') {
						error = err_BSTX;
						std::cerr << code << " : " << line << " : " << ERRORS[error];
						return error;
					}
					row_start++;
					getToNSpace(row_start, code);
				}

			}


			vector<string> f;
			for (int ij = 0; ij < DATA_FUNC.size(); ij++) {
				code.clear();
				code.push_back(DATA_FUNC[ij].second);
				code += DATA_FUNC[ij].first;
				f.push_back(code);

			}
			FNCS.push_back(pair(fnc_name, f));
			//name = "fnv_" + fnc_name;
			//g(DATA_FUNC, code, false, FUNC, STRS, 0, name);
			FUNC.push_back("fnc_");
			FUNC.back() += fnc_name;
			FUNC.back() += ':';
			//       FUNC.push_back("pop ecx");
		 //          for (int ij = DATA_FUNC.size() - 1; ij > 0; ij--) {
			//           FUNC.push_back("pop [fnv_");
			  //         FUNC.back() += fnc_name;
				//       FUNC.back() += '_';
				 //      FUNC.back() += DATA_FUNC[ij].first;
				 //      FUNC.back() += ']';
				  // }
			  //     FUNC.push_back("push ecx");
			cycles.push(pair(0, 0x81));
			GivePublics();
		}
		else if (code.substr(row_start, 6) == "cycle ") {
			row_start += 6;
			getToNSpace(row_start, code);
			for (; code.substr(row_start, 4) != " do:"; row_start++) {
				cond.push_back(code.at(row_start));
			}
			defineCondition(cond, CURRDATA(), CURROUT(), num_cond);

			if (THROW_ERROR(error, error_cause))
				return -1;

			CURROUT().push_back("push ecx");
			CURROUT().push_back("mov ecx, eax");
			CURROUT().push_back("ccl_");
			_itoa_s(num_cond, r, 10);
			CURROUT().back() += r;
			CURROUT().back() += ':';
			cycles.push(pair(num_cond, 0x80));
		}
		else if (code.substr(row_start, 6) == "until ") {
			row_start += 6;
			getToNSpace(row_start, code);
			for (; code.substr(row_start, 4) != " do:"; row_start++) {
				cond.push_back(code.at(row_start));
			}
			int place = CURROUT().size();
			defineCondition(cond, CURRDATA(), CURROUT(), num_cond);

			if (THROW_ERROR(error, error_cause))
				return -1;

			_itoa_s(num_cond, r, 10);
			name = "til_";
			name += r;
			name += ':';
			CURROUT().insert(CURROUT().begin() + place, name);
			CURROUT().push_back("test eax, eax");
			CURROUT().push_back("jnz tle_");
			CURROUT().back() += r;
			cycles.push(pair(num_cond, 0x02));
		}
		else {
			cond.clear();
			for (; row_start < code.length(); row_start++)
				cond.push_back(code.at(row_start));
			defineCondition(cond, CURRDATA(), CURROUT(), num_cond);

			if (THROW_ERROR(error, error_cause))
				return -1;
		}
	}
		
  

	g(DATA, code, is_boot, OUT, STRS, 1, "var");

	offile.open(out);

	for (int i = 0; i < OUT.size(); i++) {
		WriteString(offile, OUT[i]);
		offile.write("\n", 1);
	}
	
	/*else if (code[0] == '"') {
		str.clear();
		code.erase(0, 1);
		str += code;
		char r;
		r = iffile.get();
		while (r != '"') {
			str.push_back(r);
			r = iffile.get();
		}
		STRS.push_back(str);
		DATA.push_back(pair(" ", 0x80));*/


	offile.close();
	return 0;
}