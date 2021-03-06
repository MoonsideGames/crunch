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

 crunch - command line texture packer
 ====================================

 usage:
    crunch [OUTPUT] [INPUT1,INPUT2,INPUT3...] [OPTIONS...]

 example:
    crunch bin/atlases/atlas assets/characters,assets/tiles -p -t -v -u -r

 options:
    -d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, 256, 128, or 64)
    -p# --pad#              padding between images (# can be from 0 to 16)

 binary format:
    [int16] num_textures (below block is repeated this many times)
        [string] name
        [int16] num_images (below block is repeated this many times)
            [string] img_name
            [int16] img_x
            [int16] img_y
            [int16] img_width
            [int16] img_height
            [int16] img_frame_x         (if --trim enabled)
            [int16] img_frame_y         (if --trim enabled)
            [int16] img_frame_width     (if --trim enabled)
            [int16] img_frame_height    (if --trim enabled)
            [byte] img_rotated          (if --rotate enabled)
 */

#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "bitmap.hpp"
#include "packer.hpp"
#include "binary.hpp"
#include "hash.hpp"
#include "str.hpp"

using namespace std;
namespace fs = std::filesystem;

static int optSize;
static int optPadding;
static bool optXml;
static bool optBinary;
static bool optJson;
static bool optPremultiply;
static bool optTrim;
static bool optVerbose;
static bool optForce;
static bool optUnique;
static bool optRotate;
static vector<Bitmap*> bitmaps;
static vector<Packer*> packers;

static void LoadBitmap(fs::path directory, fs::path path)
{
    if (optVerbose)
        cout << '\t' << path << endl;

    string relativePath = (path.parent_path() / path.filename().stem()).generic_string();
    size_t pos = path.generic_string().find(directory.generic_string());
    relativePath.erase(pos, directory.string().length());

    cout << directory.generic_string() << endl;
    cout << path.generic_string() << endl;
    cout << pos << endl;

    bitmaps.push_back(new Bitmap(path.generic_string(), relativePath, optPremultiply, optTrim));
}

static void LoadBitmaps(const string& root)
{
    static string dot1 = ".";
    static string dot2 = "..";
    fs::path path = root;

    if (fs::is_directory(root))
    {
        for (fs::directory_entry const& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png")
            {
                LoadBitmap(root, entry.path().string());
            }
        }
    }
    else
    {
        if (fs::is_regular_file(root) && path.extension() == ".png")
        {
            LoadBitmap(root, path.string());
        }
    }
}

static void RemoveFile(string file)
{
    remove(file.data());
}

static int GetPackSize(const string& str)
{
    if (str == "4096")
        return 4096;
    if (str == "2048")
        return 2048;
    if (str == "1024")
        return 1024;
    if (str == "512")
        return 512;
    if (str == "256")
        return 256;
    if (str == "128")
        return 128;
    if (str == "64")
        return 64;
    cerr << "invalid size: " << str << endl;
    exit(EXIT_FAILURE);
    return 0;
}

static int GetPadding(const string& str)
{
    for (int i = 0; i <= 16; ++i)
        if (str == to_string(i))
            return i;
    cerr << "invalid padding value: " << str << endl;
    exit(EXIT_FAILURE);
    return 1;
}

int crunch_main(int argc, const char* argv[])
{
    //Print out passed arguments
    for (int i = 0; i < argc; ++i)
        cout << argv[i] << ' ';
    cout << endl;

    if (argc < 3)
    {
        cerr << "invalid input, expected: \"crunch [OUTPUT DIRECTORY] [INPUTS] [OPTIONS...]\"" << endl;
        return EXIT_FAILURE;
    }

    //Get the output directory and name
    fs::path outputPath = argv[1];
    string outputName = (outputPath.parent_path() / outputPath.filename().stem()).string();
    string relativePath = outputPath.parent_path().string();
    string name = outputPath.filename().stem().string();

    //Get all the input files and directories
    vector<string> inputs;
    stringstream ss(argv[2]);
    while (ss.good())
    {
        string inputStr;
        getline(ss, inputStr, ',');
        inputs.push_back(inputStr);
    }

    //Get the options
    optSize = 4096;
    optPadding = 1;
    optXml = false;
    optBinary = false;
    optJson = false;
    optPremultiply = false;
    optTrim = false;
    optVerbose = false;
    optForce = false;
    optUnique = false;
    for (int i = 3; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "-d" || arg == "--default")
            optXml = optPremultiply = optTrim = optUnique = true;
        else if (arg == "-x" || arg == "--xml")
            optXml = true;
        else if (arg == "-b" || arg == "--binary")
            optBinary = true;
        else if (arg == "-j" || arg == "--json")
            optJson = true;
        else if (arg == "-p" || arg == "--premultiply")
            optPremultiply = true;
        else if (arg == "-t" || arg == "--trim")
            optTrim = true;
        else if (arg == "-v" || arg == "--verbose")
            optVerbose = true;
        else if (arg == "-f" || arg == "--force")
            optForce = true;
        else if (arg == "-u" || arg == "--unique")
            optUnique = true;
        else if (arg == "-r" || arg == "--rotate")
            optRotate = true;
        else if (arg.find("--size") == 0)
            optSize = GetPackSize(arg.substr(6));
        else if (arg.find("-s") == 0)
            optSize = GetPackSize(arg.substr(2));
        else if (arg.find("--pad") == 0)
            optPadding = GetPadding(arg.substr(5));
        else if (arg.find("-p") == 0)
            optPadding = GetPadding(arg.substr(2));
        else
        {
            cerr << "unexpected argument: " << arg << endl;
            return EXIT_FAILURE;
        }
    }

    //Hash the arguments and input directories
    size_t newHash = 0;
    for (int i = 1; i < argc; ++i)
        HashString(newHash, argv[i]);
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (fs::is_directory(inputs[i]))
        {
            HashFiles(newHash, inputs[i]);
        }
        else if (fs::is_regular_file(inputs[i]))
        {
            HashFile(newHash, inputs[i]);
        }
    }

    //Load the old hash
    size_t oldHash;
    if (LoadHash(oldHash, outputName + ".hash"))
    {
        if (!optForce && newHash == oldHash)
        {
            cout << "atlas is unchanged: " << outputName << endl;
            return EXIT_SUCCESS;
        }
    }

    /*-d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, or 256)
    -p# --pad#              padding between images (# can be from 0 to 16)*/

    if (optVerbose)
    {
        cout << "options..." << endl;
        cout << "\t--xml: " << (optXml ? "true" : "false") << endl;
        cout << "\t--binary: " << (optBinary ? "true" : "false") << endl;
        cout << "\t--json: " << (optJson ? "true" : "false") << endl;
        cout << "\t--premultiply: " << (optPremultiply ? "true" : "false") << endl;
        cout << "\t--trim: " << (optTrim ? "true" : "false") << endl;
        cout << "\t--verbose: " << (optVerbose ? "true" : "false") << endl;
        cout << "\t--force: " << (optForce ? "true" : "false") << endl;
        cout << "\t--unique: " << (optUnique ? "true" : "false") << endl;
        cout << "\t--rotate: " << (optRotate ? "true" : "false") << endl;
        cout << "\t--size: " << optSize << endl;
        cout << "\t--pad: " << optPadding << endl;
    }

    //Remove old files
    RemoveFile(outputName + ".hash");
    RemoveFile(outputName + ".bin");
    RemoveFile(outputName + ".xml");
    RemoveFile(outputName + ".json");
    for (size_t i = 0; i < 16; ++i)
        RemoveFile(outputName + to_string(i) + ".png");

    //Load the bitmaps from all the input files and directories
    if (optVerbose)
        cout << "loading images..." << endl;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (fs::is_directory(inputs[i]))
        {
            LoadBitmaps(inputs[i]);
        }
    }

    //Sort the bitmaps by area
    sort(bitmaps.begin(), bitmaps.end(), [](const Bitmap* a, const Bitmap* b) {
        return (a->width * a->height) < (b->width * b->height);
    });

    //Pack the bitmaps
    while (!bitmaps.empty())
    {
        if (optVerbose)
            cout << "packing " << bitmaps.size() << " images..." << endl;
        auto packer = new Packer(optSize, optSize, optPadding);
        packer->Pack(bitmaps, optVerbose, optUnique, optRotate);
        packers.push_back(packer);
        if (optVerbose)
            cout << "finished packing: " << outputName << to_string(packers.size() - 1) << " (" << packer->width << " x " << packer->height << ')' << endl;

        if (packer->bitmaps.empty())
        {
            cerr << "packing failed, could not fit bitmap: " << (bitmaps.back())->name << endl;
            return EXIT_FAILURE;
        }
    }

    //Save the atlas image
    for (size_t i = 0; i < packers.size(); ++i)
    {
        if (optVerbose)
            cout << "writing png: " << outputName << to_string(i) << ".png" << endl;
        packers[i]->SavePng(outputName + to_string(i) + ".png");
    }

    //Save the atlas binary
    if (optBinary)
    {
        if (optVerbose)
            cout << "writing bin: " << outputName << ".bin" << endl;

        ofstream bin(outputName + ".bin", ios::binary);
        WriteShort(bin, (int16_t)packers.size());
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveBin(relativePath + to_string(i), bin, optTrim, optRotate);
        bin.close();
    }

    //Save the atlas xml
    if (optXml)
    {
        if (optVerbose)
            cout << "writing xml: " << outputName << ".xml" << endl;

        ofstream xml(outputName + ".xml");
        xml << "<atlas>" << endl;
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveXml(relativePath + to_string(i), xml, optTrim, optRotate);
        xml << "</atlas>";
    }

    //Save the atlas json
    if (optJson)
    {
        if (optVerbose)
            cout << "writing json: " << outputName << ".json" << endl;

        ofstream json(outputName + ".json");
        json << '{' << endl;
        json << "\t\"textures\":[" << endl;
        for (size_t i = 0; i < packers.size(); ++i)
        {
            json << "\t\t{" << endl;
            packers[i]->SaveJson(name + to_string(i), json, optTrim, optRotate);
            json << "\t\t}";
            if (i + 1 < packers.size())
                json << ',';
            json << endl;
        }
        json << "\t]" << endl;
        json << '}';
    }

    //Save the new hash
    SaveHash(newHash, outputName + ".hash");

    return EXIT_SUCCESS;
}
