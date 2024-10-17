#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

fs::path operator""_p(const char* data, std::size_t sz) {
    return fs::path(data, data + sz);
}

void PrintError(const std::string& include_file, const std::string& parent_file, int line_number) {
    std::cout << "unknown include file " << include_file
              << " at file " << parent_file << " at line " << line_number << std::endl;
}

bool ProcessFile(const fs::path& file_path, std::ostream& output, const std::vector<fs::path>& include_directories, const fs::path& current_dir, std::vector<fs::path>& processed_files) {
    if (std::find(processed_files.begin(), processed_files.end(), fs::canonical(file_path)) != processed_files.end()) {
        return true;  // File already processed, skip to avoid infinite recursion
    }
    processed_files.push_back(fs::canonical(file_path));

    std::ifstream input(file_path);
    if (!input) {
        return false;
    }

    static const std::regex include_regex(R"(\s*#\s*include\s*([<"]([^>"]*)[>"])\s*)");

    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        line_number++;
        std::smatch match;
        if (std::regex_match(line, match, include_regex)) {
            std::string include_file = match[2];
            bool is_local_include = match[1].str()[0] == '"';

            fs::path include_path;
            bool found = false;

            if (is_local_include) {
                include_path = current_dir / include_file;
                if (fs::exists(include_path)) {
                    found = true;
                }
            }

            if (!found) {
                for (const auto& dir : include_directories) {
                    include_path = dir / include_file;
                    if (fs::exists(include_path)) {
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                if (!ProcessFile(include_path, output, include_directories, include_path.parent_path(), processed_files)) {
                    return false;
                }
            } else {
                PrintError(include_file, file_path.string(), line_number);
                return false;
            }
        } else {
            output << line << '\n';
        }
    }

    return true;
}

bool Preprocess(const fs::path& input_path, const fs::path& output_path, const std::vector<fs::path>& include_directories) {
    std::ofstream output(output_path);
    if (!output) {
        return false;
    }

    std::vector<fs::path> processed_files;
    return ProcessFile(input_path, output, include_directories, input_path.parent_path(), processed_files);
}
string GetFileContents(string file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
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
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

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
