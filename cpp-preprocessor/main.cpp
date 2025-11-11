#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintIncludeError(const string& filename, const path& current_file_path, int current_line) {
    cout << "unknown include file " << filename << " at file "
        << current_file_path.string() << " at line " << current_line << endl;
}

bool ProcessFile(istream& in, ostream& out, const path& current_dir,
    const vector<path>& include_directories, const path& current_file_path);

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in) {
        return false;
    }

    ofstream out(out_file);
    if (!out) {
        return false;
    }

    path current_dir = in_file.parent_path();
    return ProcessFile(in, out, current_dir, include_directories, in_file);
}

bool ProcessFile(istream& in, ostream& out, const path& current_dir,
    const vector<path>& include_directories, const path& current_file_path) {
    string line;
    int current_line = 0;

    regex include_quotes(R"(\s*#\s*include\s*"([^"]*)"\s*)");
        regex include_angle(R"(\s*#\s*include\s*<([^>]*)>\s*)");

    while (getline(in, line)) {
        current_line++;
        smatch match;

        if (regex_match(line, match, include_quotes)) {
            string filename = match[1];
            path file_path = current_dir / filename;
            ifstream include_file(file_path);

            if (!include_file) {
                bool found = false;
                for (const auto& dir : include_directories) {
                    file_path = dir / filename;
                    include_file.open(file_path);
                    if (include_file) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    PrintIncludeError(filename, current_file_path, current_line);
                    return false;
                }
            }

            path new_current_dir = file_path.parent_path();
            if (!ProcessFile(include_file, out, new_current_dir, include_directories, file_path)) {
                return false;
            }
        }
        else if (regex_match(line, match, include_angle)) {
            string filename = match[1];
            path file_path;
            ifstream include_file;
            bool found = false;

            for (const auto& dir : include_directories) {
                file_path = dir / filename;
                include_file.open(file_path);
                if (include_file) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                PrintIncludeError(filename, current_file_path, current_line);
                return false;
            }

            path new_current_dir = file_path.parent_path();
            if (!ProcessFile(include_file, out, new_current_dir, include_directories, file_path)) {
                return false;
            }
        }
        else {
            out << line << endl;
        }
    }

    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}