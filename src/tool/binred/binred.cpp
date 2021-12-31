/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


int main() {
    constexpr auto def = R"(
        STRUCT:="struct" ID "{" 
        MEMBER:=ID TYPE
        TYPE:=ID FLAG?
        FLAG:="?" ID ["eq"|"bit"] INTEGER 
    )";
}