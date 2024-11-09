/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "game.h"
#include <file/file_stream.h>
#include <filesystem>
#include <fnet/util/sha256.h>
#include <number/hex/bin2hex.h>
#include <wrap/cout.h>
#include "embed.h"
#include <map>

static void set_varint_len(embed::Varint& v, std::uint64_t len) {
    if (len <= 0x3F) {
        v.prefix(0);
        v.value(len);
    }
    else if (len <= 0x3FFF) {
        v.prefix(1);
        v.value(len);
    }
    else if (len <= 0x3FFFFFFF) {
        v.prefix(2);
        v.value(len);
    }
    else {
        v.prefix(3);
        v.value(len);
    }
}

struct EmbedFileData {
    futils::file::File file;
    futils::file::MMap mmap;
    futils::view::rvec read_view;
};

struct EmbedFiles {
    EmbedFileData data;
    EmbedFileData index;
    embed::ConsistentData stat;
    std::map<futils::view::rvec, futils::view::rvec> files;
};

int load_and_verify_embed_impl(EmbedFiles& files, const char* dataFile, const char* indexFile, auto&& callback, auto&& err_callback) {
    auto data = futils::file::File::open(dataFile, futils::file::O_READ_DEFAULT);
    if (!data) {
        err_callback("failed to open ", dataFile, data.error().error<std::string>());
        return 1;
    }
    auto index = futils::file::File::open(indexFile, futils::file::O_READ_DEFAULT);
    if (!index) {
        err_callback("failed to open ", indexFile, index.error().error<std::string>());
        return 1;
    }
    auto mm_data = data->mmap(futils::file::r_perm);
    if (!mm_data) {
        err_callback("failed to mmap ", dataFile, mm_data.error().error<std::string>());
        return 1;
    }
    auto mm_index = index->mmap(futils::file::r_perm);
    if (!mm_index) {
        err_callback("failed to mmap ", indexFile, mm_index.error().error<std::string>());
        return 1;
    }
    auto read_view_data = mm_data->read_view();
    auto read_view_index = mm_index->read_view();
    if (read_view_index.empty() || read_view_data.empty()) {
        err_callback("failed to read ", dataFile, " or ", indexFile);
        return 1;
    }
    if (read_view_data.substr(0, 5) != "HWDAT" || read_view_index.substr(0, 5) != "HWIDX") {
        err_callback("invalid magic");
        return 1;
    }
    futils::binary::reader r{read_view_index};
    r.reset(read_view_index.size() - 32);
    auto saved_hash = r.remain();
    if (saved_hash.size() != 32) {
        err_callback("invalid hash size");
        return 1;
    }
    auto hashTarget = r.read();
    futils::byte hash[32]{};
    futils::sha::SHA256 idxHash, dataHash;
    idxHash.update(hashTarget);
    idxHash.update(hash);  // add 0 filled hash
    idxHash.finalize(hash);
    callback("index file hash: ", futils::number::hex::to_hex<std::string>(hash), " saved is ", futils::number::hex::to_hex<std::string>(saved_hash));
    if (saved_hash != futils::view::rvec(hash, 32)) {
        err_callback("index file hash mismatch");
        return 1;
    }
    r.reset(read_view_index.size() - embed::ConsistentData::fixed_header_size);
    auto indexConsistData = r.remain();
    r.reset_buffer(read_view_data, read_view_data.size() - 32);
    auto dataSavedHash = r.remain();
    if (dataSavedHash.size() != 32) {
        err_callback("invalid hash size");
        return 1;
    }
    r.reset(read_view_data.size() - embed::ConsistentData::fixed_header_size);
    auto dataHashTarget = r.read();
    auto dataConsistData = r.remain();
    dataHash.update(dataHashTarget);
    dataHash.update(indexConsistData);
    dataHash.finalize(hash);
    callback("data file hash: ", futils::number::hex::to_hex<std::string>(hash), " saved is ", futils::number::hex::to_hex<std::string>(dataSavedHash));
    if (dataSavedHash != futils::view::rvec(hash, 32)) {
        err_callback("data file hash mismatch");
        return 1;
    }
    embed::ConsistentData cdData, cdIndex;
    futils::binary::reader cdReader{dataConsistData};
    if (auto err = cdData.decode(cdReader)) {
        err_callback("failed to decode data file consistent data ", err.error<std::string>());
        return 1;
    }
    cdReader.reset_buffer(indexConsistData);
    if (auto err = cdIndex.decode(cdReader)) {
        err_callback("failed to decode index file consistent data ", err.error<std::string>());
        return 1;
    }
    callback("index file: files: ", cdIndex.files_len, " offset: ", cdIndex.offset);
    callback("data file: files: ", cdData.files_len, " offset: ", cdData.offset);
    if (cdData.files_len != cdIndex.files_len || cdData.offset != cdIndex.offset) {
        err_callback("inconsistent data");
        return 1;
    }
    callback("data file and index file are consistent");
    files.data.file = std::move(*data);
    files.data.mmap = std::move(*mm_data);
    files.data.read_view = read_view_data;
    files.index.file = std::move(*index);
    files.index.mmap = std::move(*mm_index);
    files.index.read_view = read_view_index;
    files.stat = cdData;
    return 0;
}

int verify_embed(const char* dataFile, const char* indexFile) {
    EmbedFiles files;
    return load_and_verify_embed_impl(files, dataFile, indexFile, [](auto&&... args) { (cout << ... << args) << "\n"; }, [](auto&&... args) { (cout << ... << args) << "\n"; });
}

int make_embed(const char* target_dir, const char* embed_file, const char* embed_index) {
    auto root_path = io::fs::path{target_dir};
    auto entries = io::fs::recursive_directory_iterator{root_path};
    if (!io::fs::exists(root_path)) {
        cout << "target directory not found\n";
        return 1;
    }
    auto embed_file_data = futils::file::File::create(embed_file);
    if (!embed_file_data) {
        cout << "failed to create " << embed_file << ": " << embed_file_data.error().error<std::string>() << "\n";
        return 1;
    }
    auto embed_index_data = futils::file::File::create(embed_index);
    if (!embed_index_data) {
        cout << "failed to create " << embed_index << ": " << embed_index_data.error().error<std::string>() << "\n";
        return 1;
    }
    futils::file::FileStream<std::string> efs{*embed_file_data}, ifs{*embed_index_data};
    futils::binary::writer ew{efs.get_direct_write_handler(), &efs}, iw{ifs.get_direct_write_handler(), &ifs};
    std::uint64_t offset = 0;
    std::uint64_t file_count = 0;
    futils::sha::SHA256 idxHash, dataHash;
    if (!iw.write("HWIDX")) {
        cout << "fatal: failed to write magic\n";
        return 1;
    }
    if (!ew.write("HWDAT")) {
        cout << "fatal: failed to write magic\n";
        return 1;
    }
    idxHash.update("HWIDX");
    dataHash.update("HWDAT");
    for (auto entry : entries) {
        if (entry.is_regular_file()) {
            auto path = entry.path();
            auto data = futils::file::File::open(path.u8string(), futils::file::O_READ_DEFAULT);
            if (!data) {
                cout << "failed to open " << path.u8string() << data.error().error<std::string>() << "\n";
                continue;
            }
            auto res = data->mmap(futils::file::r_perm);
            if (!res) {
                cout << "warning: failed to mmap " << path.u8string() << res.error().error<std::string>() << "\n";
                continue;
            }
            auto read_view = res->read_view();
            if (read_view.empty()) {
                cout << "warning: failed to read " << path.u8string() << "\n";
                continue;
            }
            auto relative_path = io::fs::relative(path, root_path);
            auto u8path = relative_path.generic_u8string();
            embed::EmbedFileIndex file;
            set_varint_len(file.name.len, u8path.size());
            file.name.name = futils::view::rvec{u8path};
            set_varint_len(file.len, read_view.size());
            set_varint_len(file.offset, offset);
            std::string tmp_index_buffer;
            futils::binary::writer tmp{futils::binary::resizable_buffer_writer<std::string>(), &tmp_index_buffer};
            if (auto err = file.encode(tmp)) {
                cout << "fatal: failed to encode " << path.u8string() << err.error<std::string>() << "\n";
                return 1;
            }
            if (!iw.write(tmp.written())) {
                cout << "fatal: failed to encode " << path.u8string() << "\n";
                return 1;
            }
            if (!ew.write(read_view)) {
                cout << "fatal: failed to write " << path.u8string() << "\n";
                return 1;
            }
            idxHash.update(tmp.written());
            dataHash.update(read_view);
            file_count++;
            offset += read_view.size();
        }
    }
    embed::ConsistentData cd{};
    cd.files_len = file_count;
    cd.offset = offset;
    std::string tmp_index_buffer;
    futils::binary::writer tmp{futils::binary::resizable_buffer_writer<std::string>(), &tmp_index_buffer};
    if (auto err = cd.encode(tmp)) {  // for hash calculation
        cout << "fatal: failed to encode consistent data " << err.error<std::string>() << "\n";
        return 1;
    }
    // consistent data with 0 filled hash
    idxHash.update(tmp.written());
    idxHash.finalize(cd.hash);
    cout << "index file: " << futils::number::hex::to_hex<std::string>(futils::view::rvec(cd.hash)) << "\n";
    tmp.reset(0);
    if (auto err = cd.encode(tmp)) {  // data including hash
        cout << "fatal: failed to encode consistent data " << err.error<std::string>() << "\n";
        return 1;
    }
    if (!iw.write(tmp.written())) {  // write consistent data
        cout << "fatal: failed to encode consistent data " << "\n";
        return 1;
    }
    // by using index file hash for data hash,
    // we can ensure that the data is consistent with the index file
    dataHash.update(tmp.written());
    dataHash.finalize(cd.hash);
    cout << "data file: " << futils::number::hex::to_hex<std::string>(futils::view::rvec(cd.hash)) << "\n";
    if (auto err = cd.encode(ew)) {
        cout << "fatal: failed to encode consistent data " << err.error<std::string>() << "\n";
        return 1;
    }
    return 0;
}

static EmbedFiles embed_files;

void unload_embed() {
    embed_files = {};
}

int load_embed(const char* dataFile, const char* indexFile, io::TextController& text) {
    auto res = load_and_verify_embed_impl(
        embed_files, dataFile, indexFile,
        [](auto&&...) {
            // ignore
        },
        [&](auto&&... args) {
            auto raw = futils::wrap::packln(args...).raw();
            text.write_story(U"エラーが発生しました。\n" + futils::utf::convert<std::u32string>(raw), 1);
        });
    if (res != 0) {
        return res;
    }
    futils::binary::reader r{embed_files.index.read_view};
    r.offset(5);  // skip magic
    for (size_t i = 0; i < embed_files.stat.files_len; i++) {
        embed::EmbedFileIndex index;
        if (auto err = index.decode(r)) {
            text.write_story(U"エラーが発生しました。\n" + futils::utf::convert<std::u32string>(err.error<std::string>()), 1);
            return 1;
        }
        auto offset = 5 + index.offset.value();  // skip magic
        auto data = embed_files.data.read_view.substr(offset, index.len.value());
        if (data.size() != index.len.value()) {
            text.write_story(U"エラーが発生しました。\n" + futils::utf::convert<std::u32string>("data size mismatch"), 1);
            return 1;
        }
        embed_files.files.emplace(index.name.name, data);
    }
    return 0;
}

futils::view::rvec read_file(futils::view::rvec file, io::TextController& err) {
    auto found = embed_files.files.find(file);
    if (found == embed_files.files.end()) {
        auto fileNameU32 = futils::utf::convert<std::u32string>(file);
        err.write_story(U"エラーが発生しました。\n" + fileNameU32 + U"がみつかりません", 1);
        return {};
    }
    return found->second;
}