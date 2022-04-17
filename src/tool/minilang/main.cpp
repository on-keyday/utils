
#include "minilang.h"
#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <helper/line_pos.h>
using namespace utils::cmdline;
namespace hlp = utils::helper;
namespace wrap = utils::wrap;
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

struct CmdLineOption {
    wrap::string input;
    bool dbginfo = false;
    void bind(option::Context& ctx) {
        ctx.VarString(&input, "input,i", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
        ctx.VarBool(&dbginfo, "debug-info,d", "show debug info");
    }
};

int main(int argc, char** argv) {
    CmdLineOption opt;
    option::Context ctx;
    opt.bind(ctx);
    auto perr = option::parse_required(argc, argv, ctx, hlp::nop, option::ParseFlag::assignable_mode);
    auto report = [](auto... err) {
        cerr << "minilang: error: ";
        (cerr << ... << err);
        cerr << "\n";
    };
    if (auto msg = error_msg(perr)) {
        report(msg);
        return -1;
    }
    minilang::expr::Expr* expr = nullptr;
    utils::file::View view;
    if (!view.open(opt.input)) {
        report("failed to open file ", opt.input);
        return -1;
    }
    auto seq = utils::make_ref_seq(view);
    minilang::expr::Errors<wrap::string, wrap::vector> errs;
    if (!minilang::parse(seq, expr, errs)) {
        report("failed to parse file ", opt.input);
        cerr << "failed locations\n";
        for (auto& v : errs.stack) {
            cerr << "type: " << v.type << "\n";
            cerr << v.msg << "\n";
            if (opt.dbginfo) {
                cout << "src: " << v.loc << ":" << v.line << "\n";
            }
            wrap::string err;
            seq.rptr = v.end;
            hlp::write_src_loc(err, seq);
            cout << err << "\n";
        }
        return -1;
    }
    minilang::Scope scope;
    auto node = minilang::convert_to_node(expr, &scope, true);
    minilang::runtime::Interpreter inpret;
    if (!inpret.eval(node)) {
        report("eval error!");
        for (auto& v : inpret.stack) {
            cerr << v.msg << "\n";
            seq.rptr = v.node->expr->pos();
            wrap::string err;
            hlp::write_src_loc(err, seq);
            cout << err << "\n";
        }
        return -1;
    }
    return 0;
}
