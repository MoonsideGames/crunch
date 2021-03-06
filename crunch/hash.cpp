/*

 MIT License

 Copyright (c) 2017 Chevy Ray Johnston

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 */

#include "hash.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>
#include <limits>
#include <filesystem>
#include "str.hpp"

using namespace std;
namespace fs = std::filesystem;

template <class T>
void HashCombine(std::size_t& hash, const T& v)
{
    std::hash<T> hasher;
    hash ^= hasher(v) + 0x9e3779b9 + (hash<<6) + (hash>>2);
}
void HashCombine(std::size_t& hash, size_t v)
{
    hash ^= v + 0x9e3779b9 + (hash<<6) + (hash>>2);
}

void HashString(size_t& hash, const string& str)
{
    HashCombine(hash, str);
}

void HashFile(size_t& hash, const string& file)
{
    ifstream filestream;
    filestream.open(file, ios::binary | ios::ate);
    filestream.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize length = filestream.gcount();
    filestream.clear();
    filestream.seekg(0, ios::beg);
    vector<char> buffer(length + 1);
    if (!filestream.read(buffer.data(), length))
    {
        cerr << "failed to read file: " << file << endl;
        exit(EXIT_FAILURE);
    }
    buffer[length] = '\0';
    string text(buffer.begin(), buffer.end());
    HashCombine(hash, text);
}

void HashFiles(size_t& hash, const string& root)
{
    static string dot1 = ".";
    static string dot2 = "..";
    fs::path path = root;

    if (fs::is_directory(root))
    {
        for (fs::directory_entry const& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png")
            {
                HashFile(hash, entry.path().string());
            }
        }
    }
    else
    {
        if (fs::is_regular_file(root) && path.extension() == ".png")
        {
            HashFile(hash, path.string());
        }
    }
}

void HashData(size_t& hash, const char* data, size_t size)
{
    string str(data, size);
    HashCombine(hash, str);
}

bool LoadHash(size_t& hash, const string& file)
{
    ifstream stream(file);
    if (stream)
    {
        stringstream ss;
        ss << stream.rdbuf();
        ss >> hash;
        return true;
    }
    return false;
}

void SaveHash(size_t hash, const string& file)
{
    ofstream stream(file);
    stream << hash;
}
