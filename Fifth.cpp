#include "Fifth.h"

#include "cstdio.h"

#include <cmath>
#include <cstddef>

namespace Fifth {

static constexpr Real Half = 0.5;

static std::wstring toString(VM* vm, Compiled* code, const Value& v) {
    switch (v.index()) {
    case INTEGER:  return std::to_wstring(std::get<Integer>(v));
    case REAL:     return std::to_wstring(std::get<Real>(v));
    case STRING:   return L"'" + std::get<String>(v) + L"'";
    case EXTERNAL: return L"(x)";
    case VALUEPTR: {
            Value* ptr = (Value*) std::get<void*>(v);                                      // NOLINT
            if (code->reverse().contains(ptr)) return L"local:" + code->reverse()[ptr];
            else if (vm->reverse().contains(ptr)) return L"global:" + vm->reverse()[ptr];
            else return L"*" + std::to_wstring((size_t) ptr);                              // NOLINT
        }
    }
    return L"";
}

static void applyOperator(VM* vm, Value op) {
    String name = std::get<String>(op);
    if (vm->compiling()) {
        Value right = vm->pop();
        Value left = vm->pop();
        auto* code = dynamic_cast<Compiled*>(vm->code());
        if (left.index() != VALUEPTR || std::get<VALUEPTR>(left) != nullptr) {
            if (left.index() == STRING) {
                String name = std::get<STRING>(left);
                if (vm->dictionary().contains(name)) code->call(vm->dictionary()[name]);
            } else {
                code->push(left);
                if (left.index() == VALUEPTR) code->call(vm->dictionary()[L"get"]);
            }
        }
        if (right.index() != VALUEPTR || std::get<VALUEPTR>(right) != nullptr) {
            if (right.index() == STRING) {
                String name = std::get<STRING>(right);
                if (vm->dictionary().contains(name)) code->call(vm->dictionary()[name]);
            } else {
                code->push(right);
                if (right.index() == VALUEPTR) code->call(vm->dictionary()[L"get"]);
            }
        }
        code->call(vm->dictionary()[name]);
        vm->push((Value*) nullptr);          // Dummy value to keep alorithm happy, but we know not to pus it in the code
    } else vm->dictionary()[name]->exec(vm);
}

static bool hasHigherPrecedence(std::map<Value, int>& precedence, Value op1, Value op2) {
    return precedence[op1] > precedence[op2];
}

void add(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        switch (left.index()) {
        case INTEGER:  vm->push(Integer(asReal(left) + asReal(right) + Half));          break;
        case REAL:     vm->push(asReal(left) + asReal(right));                          break;
        case STRING:   vm->push(std::get<STRING>(left) + asString(right));              break;
        case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"+", right); vm->push(left); break;
        case VALUEPTR:
            if (right.index() > REAL) vm->push(left);
            else {
                Value* valptr = (Value*) std::get<VALUEPTR>(left);                              // NOLINT
                Integer idx = asInteger(right);
                valptr += idx;                                                                  // NOLINT
                vm->push((void*) valptr);                                                       // NOLINT
            }
            break;
        }
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) + std::get<INTEGER>(right));      break;
    case REAL:     vm->push(std::get<REAL>(left) + std::get<REAL>(right));            break;
    case STRING:   vm->push(std::get<STRING>(left) + std::get<STRING>(right));        break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"+", right); vm->push(left);   break;
    case VALUEPTR: vm->push(left);                                                    break;
    }
}

void array(VM* vm) {
    if (auto val = vm->word(true); val.has_value()) {
        vm->pop();
        if (Value value = val.value(); value.index() == STRING) {
            auto name = std::get<String>(value);
            if (auto val = vm->word(true); val.has_value()) {
                vm->pop();
                if (Value value = val.value(); value.index() == INTEGER) {
                    auto size = std::get<Integer>(value);
                    if (vm->compiling()) {
                        vm->code()->locals()[name] = { 0 };
                        vm->code()->locals()[name].resize(size);
                        vm->code()->reverse()[vm->code()->locals()[name].data()] = name;
                        return;
                    }
                    vm->globals()[name] = { 0 };
                    vm->globals()[name].resize(size);
                    vm->reverse()[vm->globals()[name].data()] = name;
                }
            }
        }
    }
}

std::map<Value, int> precedence; // NOLINT

void algebra(VM* vm) {
    if (precedence.empty()) {
        constexpr int BOOL = 10;
        constexpr int COMP = 20;
        constexpr int ADD  = 30;
        constexpr int MULT = 40;
        constexpr int POW  = 50;
        precedence[L"and"]  = BOOL;
        precedence[L"or"]   = BOOL;
        precedence[L"nand"] = BOOL;
        precedence[L"nor"]  = BOOL;
        precedence[L"xor"]  = BOOL;
        precedence[L"<="]   = COMP;
        precedence[L"<"]    = COMP;
        precedence[L"="]    = COMP;
        precedence[L"!="]   = COMP;
        precedence[L"<>"]   = COMP;
        precedence[L">"]    = COMP;
        precedence[L">="]   = COMP;
        precedence[L"+"]    = ADD;
        precedence[L"-"]    = ADD;
        precedence[L"*"]    = MULT;
        precedence[L"/"]    = MULT;
        precedence[L"%"]    = MULT;
        precedence[L"^"]    = POW;
    }
    vm->syspush(L"[");

    for (auto tkn = vm->word(true); tkn.has_value(); tkn = vm->word()) {
        vm->pop();
        Value token = tkn.value();
        if (token.index() == INTEGER || token.index() == REAL) vm->push(token);
        else {
            if (token.index() == STRING) {
                String op = std::get<String>(token);
                if (op == L"(") vm->syspush(token);
                else if (op == L")") {
                    Value systop = vm->systop();
                    String top = std::get<String>(systop);
                    while (!(top == L"[" || top == L"(")) {
                        applyOperator(vm, systop);
                        vm->syspop();
                        systop = vm->systop();
                        top = std::get<String>(systop);
                    }
                    if (top == L"[") break;
                    vm->syspop(); // Remove '(' or '['
                } else if (precedence.contains(token)){
                    Value systop = vm->systop();
                    String top = std::get<String>(systop);
                    while (top != L"[" && hasHigherPrecedence(precedence, systop, token)) {
                        applyOperator(vm, systop);
                        vm->syspop();
                        systop = vm->systop();
                        top = std::get<String>(systop);
                    }
                    vm->syspush(token);
                } else {
                    String var = std::get<STRING>(token);
                    if (var[0] == '*') {
                        var = var.substr(1);
                        if (vm->globals().contains(var)) {
                            if (vm->compiling()) vm->push((Value*) vm->globals()[var].data());
                            else vm->push(*(vm->globals()[var].data()));
                        } else if (vm->code()->locals().contains(var)) {
                            if (vm->compiling()) vm->push((Value*) vm->code()->locals()[var].data());
                            else vm->push(*(vm->code()->locals()[var].data()));
                        }
                    } else {
                        if (vm->globals().contains(var)) vm->push(vm->globals()[var].data());
                        else if (vm->code()->locals().contains(var)) vm->push(vm->code()->locals()[var].data());
                        else vm->push(token);
                    }
                }
            } else vm->push(token);
        }
    }
    while (asString(vm->systop()) != L"[") applyOperator(vm, vm->syspop());
    vm->syspop();
    if (vm->compiling()) vm->pop();
}

//
//    $ syspop                        [ (var) 0 ]            [ ]                  [ ]
//    syspop                          [ (var) ]              [ ]                  [ ]
//    syspush 1                       [ (var) 1 ]            [ ]                  [ ]
//
void by(VM* vm) {
    if (!vm->compiling()) return;

    auto* code = dynamic_cast<Compiled*>(vm->code());
    code->syspop();
    vm->syspop();
    vm->syspush(0);
}

void dbg(VM* vm) {
    if (auto val = vm->word(true); val.has_value()) {
        vm->pop();
        if (vm->compiling()) return;
        if (Value value = val.value(); value.index() == STRING) {
            auto& dict = vm->dictionary();
            auto& globals = vm->globals();
            auto name = std::get<String>(value);
            if (dict.contains(name)) {
                if (auto* block = dynamic_cast<Compiled*>(dict[name]); block) {
                    size_t sz = block->size();
                    for (size_t i = 0; i < sz; ++i) {
                        cstd::out.print("%4d ", i);                                                                                        // NOLINT
                        const auto& instr = block->get(i);
                        switch (instr.op()) {
                        case Compiled::NOP:     cstd::out.putString(L"NOP\r\n");                                                 break;
                        case Compiled::POP:     cstd::out.putString(L"POP\r\n");                                                 break;
                        case Compiled::SYSPOP:  cstd::out.putString(L"SYSPOP\r\n");                                              break;
                        case Compiled::RETURN:  cstd::out.putString(L"RETURN\r\n");                                              break;
                        case Compiled::PUSH:    cstd::out.putString(L"PUSH " + toString(vm, block, instr.value()) + L"\r\n");    break;
                        case Compiled::SYSPUSH: cstd::out.putString(L"SYSPUSH " + toString(vm, block, instr.value()) + L"\r\n"); break;
                        case Compiled::JUMP:    cstd::out.putString(L"JUMP " + asString(instr.by()) + L"\r\n");                  break;
                        case Compiled::BRANCH:  cstd::out.putString(L"BRANCH " + asString(instr.by()) + L"\r\n");                break;
                        case Compiled::CALL:
                            auto* code = instr.code();
                            String out = L"<unknown>";
                            if (String nm = vm->nameOf(code); !nm.empty()) out = nm;
                            cstd::out.putString(L"CALL " + out + L"\r\n");
                            break;
                        }
                    }
                } else cstd::out.putString(name + L": builtin\r\n");
            } else if (globals.contains(name)) cstd::out.putString(name + L": " + asString((void*) globals[name].data()) + L"\r\n"); // NOLINT
            else cstd::out.putString(asString(value) + L"\r\n");
        } else cstd::out.putString(asString(value) + L"\r\n");
    }
}

void def(VM* vm) {
    vm->compiling(true);
    if (auto val = vm->word(true); val.has_value()) {
        vm->pop();
        if (Value value = val.value(); value.index() == STRING) {
            auto& dict = vm->dictionary();
            auto& globals = vm->globals();
            auto name = std::get<String>(value);
            auto block = new Compiled();                                  // NOLINT
            Code* save = vm->code();
            vm->code(block);
            auto& locals = block->locals();
            for (; ; ) {
                val = vm->word();
                if (!val.has_value()) {
                    delete block;                                         // NOLINT
                    block = nullptr;
                    break;
                }
                value = val.value();
                vm->pop();

                if (value.index() == STRING) {
                    if (String word = std::get<String>(value); word == L"end") {
                        block->ret();
                        break;
                    } else if (String word = std::get<String>(value); word == L"return") block->ret();
                    else if (dict.contains(word)) {
                        Code* code = dict[word];
                        if (code->immediate()) code->exec(vm);
                        else block->call(dict[word]);
                    } else if (globals.contains(word)) block->push((Value*) globals[word].data());
                    else if (locals.contains(word)) block->push((Value*) locals[word].data());
                    else block->push(value);
                } else block->push(value);
            }
            if (block) {
                vm->dictionary()[name] = block;
                vm->nameOf(block, name);
            }
            vm->code(save);
        }
    }
    vm->compiling(false);
}

void divide(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        switch (left.index()) {
        case INTEGER:  vm->push(Integer(asReal(left) / asReal(right) + Half));          break;
        case REAL:     vm->push(asReal(left) / asReal(right));                          break;
        case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"/", right); vm->push(left); break;
        case VALUEPTR: vm->push(left);                                                  break;
        case STRING: {
                switch (right.index()) {
                case INTEGER: {
                        String str = get<STRING>(left);
                        Integer x = std::get<INTEGER>(right);
                        Integer num = 0;
                        while (str.size() > size_t(x)) {
                            String front = str.substr(0, x);
                            vm->push(front);
                            ++num;
                            str = str.substr(x);
                        }
                        vm->push(str);
                        ++num;
                        vm->push(num);
                    }
                    break;
                case REAL: {
                        String str = get<STRING>(left);
                        Real x = std::get<Real>(right);
                        Real p = 0;
                        Integer num = 0;
                        while (Real(str.size()) - p > x) {
                            String part = str.substr(size_t(p + Half), size_t(p + x + Half) - size_t(p));
                            vm->push(part);
                            p += x;
                            ++num;
                        }
                        vm->push(str.substr(Integer(p + Half)));
                        ++num;
                        vm->push(num);
                    }
                    break;
                default:
                    vm->push(left);
                }
            }
            break;
        }
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) / std::get<INTEGER>(right));    break;
    case REAL:     vm->push(std::get<REAL>(left) / std::get<REAL>(right));          break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"/", right); vm->push(left); break;
    case VALUEPTR: vm->push(left);                                                  break;
    case STRING: {
            String str = get<STRING>(left);
            String x = std::get<String>(right);
            size_t pos = 0;
            Integer num = 0;
            while ((pos = str.find(x)) != std::string::npos) {
                String part = str.substr(0, pos);
                str = str.substr(pos + x.size());
                vm->push(part);
                ++num;
            }
            vm->push(str);
            ++num;
            vm->push(num);
        }
        break;
    }
}

void doDo(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    code->branch(1);
    Integer location = Integer(code->jump(0));
    vm->syspush(location);
}

void doElse(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    Integer loc = Integer(code->jump(0));
    Integer prev = asInteger(vm->syspop());
    code->update(int(prev), int(loc - prev));
    vm->syspush(loc);
}

void doEndIf(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    Integer loc = Integer(code->location());
    Integer prev = asInteger(vm->syspop());
    code->update(int(prev), int(loc - prev));
}

//
//                                    [ ]                    [ ]                  [ ]
//    (var) = word                    [ ]                    [ ]                  [ ]
//    create local (var)              [ ]                    [ ]                  [ ]
//    syspush (var)                   [ (var) ]              [ ]                  [ ]
//    $ syspush 1                     [ (var) ]              [ ]                  [ 1 ]
//    syspush 1                       [ (var) 1 ]            [ ]                  [ 1 ]
//
void doFor(VM* vm) {
    if (!vm->compiling()) return;

    if (auto val = vm->word(true); val.has_value()) {
        vm->pop();
        if (Value value = val.value(); value.index() == STRING) {
            auto name = std::get<String>(value);
            Compiled* code = dynamic_cast<Compiled*>(vm->code());
            code->locals()[name] = { 0 };
            code->reverse()[vm->code()->locals()[name].data()] = name;
            vm->syspush((Value*) vm->code()->locals()[name].data());
            code->syspush(1);
            vm->syspush(1);
        }
    }
}

void doReturn(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    code->ret();
}

void doWhile(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    Integer loc = Integer(code->location());
    vm->syspush(loc);
}

void done(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    Integer location = Integer(code->location());
    Integer jump = asInteger(vm->syspop());
    Integer start = asInteger(vm->syspop());
    code->update(int(jump), int(location - jump + 1));
    code->jump(int(start - location - 1));
}

void equal(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(!res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) == std::get<INTEGER>(right));   break;
    case REAL:     vm->push(std::get<REAL>(left) == std::get<REAL>(right));         break;
    case STRING:   vm->push(std::get<STRING>(left) == std::get<STRING>(right));     break;
    case EXTERNAL: vm->push(std::get<EXTERNAL>(left) == std::get<EXTERNAL>(right)); break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) == std::get<VALUEPTR>(right)); break;
    }
}

void explode(VM* vm) {
    if (vm->size() < 1) return;
    Value val = vm->top();
    if (val.index() != STRING) return;
    vm->pop();
    String str = std::get<STRING>(val);
    for (const auto ch: str) {
        wchar_t buffer[2] = { 0, 0 };      // NOLINT
        buffer[0] = ch;
        vm->push(String(buffer));          // NOLINT
    }
    vm->push(Integer(str.size()));
}

void get(VM* vm) {
    if (vm->empty()) return;
    Value value = vm->pop();
    if (value.index() != VALUEPTR) return;
    Value* var = (Value*) std::get<void*>(value); // NOLINT
    vm->push(*var);
}

void greater(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(!res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) > std::get<INTEGER>(right));               break;
    case REAL:     vm->push(std::get<REAL>(left) > std::get<REAL>(right));                     break;
    case STRING:   vm->push(std::get<STRING>(left) > std::get<STRING>(right));                 break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L">", right); vm->push(left);            break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) > std::get<VALUEPTR>(right));             break;
    }
}

void greaterEqual(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(!res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) >= std::get<INTEGER>(right));               break;
    case REAL:     vm->push(std::get<REAL>(left) >= std::get<REAL>(right));                     break;
    case STRING:   vm->push(std::get<STRING>(left) >= std::get<STRING>(right));                 break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L">=", right); vm->push(left);            break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) >= std::get<VALUEPTR>(right));             break;
    }
}

void len(VM* vm) {
    if (vm->size() < 1) return;
    Value val = vm->pop();
    if (val.index() != STRING) {
        vm->push(0);
        return;
    }
    String str = std::get<STRING>(val);
    vm->push(Integer(str.size()));
}

void less(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(!res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) < std::get<INTEGER>(right));               break;
    case REAL:     vm->push(std::get<REAL>(left) < std::get<REAL>(right));                     break;
    case STRING:   vm->push(std::get<STRING>(left) < std::get<STRING>(right));                 break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"<", right); vm->push(left);            break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) < std::get<VALUEPTR>(right));             break;
    }
}

void lessEqual(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(!res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) <= std::get<INTEGER>(right));               break;
    case REAL:     vm->push(std::get<REAL>(left) <= std::get<REAL>(right));                     break;
    case STRING:   vm->push(std::get<STRING>(left) <= std::get<STRING>(right));                 break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"<=", right); vm->push(left);            break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) <= std::get<VALUEPTR>(right));             break;
    }
}

void modulo(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        switch (left.index()) {
        case INTEGER:                                                                                   break;
        case REAL:     vm->push(asInteger(right) ? abs(long(asInteger(left) % asInteger(right))) : -1); break;
        case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"%", right); vm->push(left);                 break;
        case VALUEPTR: vm->push(left);                                                                  break;
        case STRING:   vm->push(left);                                                                  break;
        }
        return;
    }
    switch (left.index()) {
    case INTEGER:
    case REAL:     vm->push(asInteger(left) % asInteger(right));                    break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"%", right); vm->push(left); break;
    case VALUEPTR: vm->push(left);                                                  break;
    case STRING:   vm->push(left);                                                  break;
    }
}

void multiply(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        switch (left.index()) {
        case INTEGER:  vm->push(Integer(asReal(left) * asReal(right) + Half));          break;
        case REAL:     vm->push(asReal(left) * asReal(right));                          break;
        case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"*", right); vm->push(left); break;
        case VALUEPTR: vm->push(left);                                                  break;
        case STRING: {
                switch (right.index()) {
                case INTEGER: {
                        String s;
                        Integer n = std::get<INTEGER>(right);
                        String l = std::get<String>(left);
                        for (auto i = 0; i < n; ++i) s += l;
                        vm->push(s);
                    }
                    return;
                case REAL: {
                        String s;
                        Real r = std::get<REAL>(right);
                        Integer n = r;                       // NOLINT
                        String l = std::get<String>(left);
                        for (auto i = 0; i < n; ++i) s += l;
                        n = l.size() * (r - n);              // NOLINT
                        s += l.substr(0, n);
                        vm->push(s);
                    }
                    break;
                }
                vm->push(left);
            }
            break;
        }
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) * std::get<INTEGER>(right));    break;
    case REAL:     vm->push(std::get<REAL>(left) * std::get<REAL>(right));          break;
    case STRING:   vm->push(left);                                                  break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"*", right); vm->push(left); break;
    case VALUEPTR: vm->push(left);                                                  break;
    }
}

//
//                                    [ (var) (loc) (loc2)   [ ]                  [ (by) (to) ]
//    (loc2) = syspop                 [ (var) (loc) ]        [ ]                  [ (by) (to) ]
//    (loc) = syspop                  [ (var) ]              [ ]                  [ (by) (to) ]
//    (var) = syspop                  [ ]                    [ ]                  [ (by) (to) ]
//    $ push (var)                    [ ]                    [ (var) ]            [ (by) (to) ]
//    $ dup                           [ ]                    [ (var) (var) ]      [ (by) (to) ]
//    $ sysover                       [ ]                    [ (var) (var) ]      [ (by) (to) (by) ]
//    $ call get                      [ ]                    [ (var) <res> ]      [ (by) (to) (by) ]
//    $ call move                     [ ]                    [ (var) <res> (by) ] [ (by) (to) ]
//    $ call +                        [ ]                    [ (var) <res> ]      [ (by) (to) ]
//    $ call <-                       [ ]                    [ ]                  [ (by) (to) ]
//    $ (loc3) = jump (loc)           [ ]                    [ ]                  [ (by) (to) ]
//    update jump at (loc2) to (loc3) [ ]                    [ ]                  [ (by) (to) ]
//    $ syspop                        [ ]                    [ ]                  [ (by) ]
//    $ syspop                        [ ]                    [ ]                  [ ]
//
void next(VM* vm) {
    if (!vm->compiling()) return;

    Value loc2 = vm->syspop();
    Value loc = vm->syspop();
    Value var = vm->syspop();
    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    code->push(var);
    code->call(vm->dictionary()[L"dup"]);
    code->call(vm->dictionary()[L"sysover"]);
    code->call(vm->dictionary()[L"get"]);
    code->call(vm->dictionary()[L"move"]);
    code->call(vm->dictionary()[L"+"]);
    code->call(vm->dictionary()[L"<-"]);
    size_t loc3 = code->location();
    code->jump(int(asInteger(loc) - loc3 - 2));
    code->update(int(asInteger(loc2)), int(loc3 - asInteger(loc2) + 1));
    code->call(vm->dictionary()[L"syspop"]);
    code->call(vm->dictionary()[L"syspop"]);
}

void notEqual(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        vm->push(res);
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) != std::get<INTEGER>(right));               break;
    case REAL:     vm->push(std::get<REAL>(left) != std::get<REAL>(right));                     break;
    case STRING:   vm->push(std::get<STRING>(left) != std::get<STRING>(right));                 break;
    case EXTERNAL: vm->push(std::get<EXTERNAL>(left) != std::get<EXTERNAL>(right));             break;
    case VALUEPTR: vm->push(std::get<VALUEPTR>(left) != std::get<VALUEPTR>(right));             break;
    }
}

void power(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();
    if (right.index() > REAL || left.index() > REAL) {
        vm->push(left);
        return;
    }
    Real n1 = asReal(left);
    Real n2 = asReal(right);
    vm->push(pow(n1, n2));                               // NOLINT
}

//
//                                    [ (var) 1 ]            [ (start) (t) ?(b) ]  [ ?1 ]
//    if (syspop == 1) $ move         [ (var) ]              [ (start) (to) (by) ] [ ]
//    $ sysmove                       [ (var) ]              [ (start) (to) ]      [ (by) ]
//    $ sysmove                       [ (var) ]              [ (start) ]           [ (by) (to) ]
//    (var) = systop                  [ (var) ]              [ (start) ]           [ (by) (to) ]
//    $ push (var)                    [ (var) ]              [ (start) (var) ]     [ (by) (to) ]
//    $ call ->                       [ (var) ]              [ ]                   [ (by) (to) ]
//    $ (loc) = sysdup                [ (var) ]              [ ]                   [ (by) (to) (to) ]
//    syspush (loc)                   [ (var) (loc) ]        [ ]                   [ (by) (to) (to) ]
//    $ call move                     [ (var) (loc) ]        [ (to) ]              [ (by) (to) ]
//    $ push (var)                    [ (var) (loc) ]        [ (to) (var) ]        [ (by) (to) ]
//    $ call get                      [ (var) (loc) ]        [ (to) <res> ]        [ (by) (to) ]
//    $ call sysover                  [ (var) (loc) ]        [ <res> ]             [ (by) (to) (by) ]
//    $ call move                     [ (var) (loc) ]        [ <res> (by) ]        [ (by) (to) ]
//    $ push 0                        [ (var) (loc) ]        [ <res> (by) 0 ]      [ (by) (to) ]
//    $ call '>'                      [ (var) (loc) ]        [ <res> <res> ]       [ (by) (to) ]
//    $ branch 2                      [ (var) (loc) ]        [ <res> ]             [ (by) (to) ]
//    $ call '<='                     [ (var) (loc) ]        [ ]                   [ (by) (to) ]
//    $ JUMP 1                        [ (var) (loc) ]        [ ]                   [ (by) (to) ]
//    $ call '>='                     [ (var) (loc) ]        [ <res> ]             [ (by) (to) ]
//    $ branch 1                      [ (var) (loc) ]        [ ]                   [ (by) (to) ]
//    $ (loc2) = jump 0               [ (var) (loc) ]        [ ]                   [ (by) (to) ]
//    syspush (loc2)                  [ (var) (loc) (loc2) ] [ ]                   [ (by) (to) ]
//
void step(VM* vm) {
    if (!vm->compiling()) return;

    Compiled* code = dynamic_cast<Compiled*>(vm->code());
    if (asInteger(vm->syspop()) == 1) code->call(vm->dictionary()[L"move"]);
    code->call(vm->dictionary()[L"sysmove"]);
    code->call(vm->dictionary()[L"sysmove"]);
    Value var = vm->systop();
    code->push(var);
    code->call(vm->dictionary()[L"->"]);
    size_t loc = code->call(vm->dictionary()[L"sysdup"]);
    vm->syspush(Integer(loc));
    code->call(vm->dictionary()[L"move"]);
    code->push(var);
    code->call(vm->dictionary()[L"get"]);
    code->call(vm->dictionary()[L"sysover"]);
    code->call(vm->dictionary()[L"move"]);
    code->push(0);
    code->call(vm->dictionary()[L">"]);
    code->branch(2);
    code->call(vm->dictionary()[L"<="]);
    code->jump(1);
    code->call(vm->dictionary()[L">="]);
    code->branch(1);
    size_t loc2 = code->jump(0);
    vm->syspush(Integer(loc2));
}

void storeLeft(VM* vm) {
    if (vm->size() < 2) return;
    Value value = vm->pop();
    Value var = vm->pop();
    if (var.index() != VALUEPTR) return;
    Value* varPtr = (Value*) std::get<VALUEPTR>(var);     // NOLINT
    *varPtr = value;
}

void storeRight(VM* vm) {
    if (vm->size() < 2) return;
    Value var = vm->pop();
    Value value = vm->pop();
    if (var.index() != VALUEPTR) return;
    Value* varPtr = (Value*) std::get<VALUEPTR>(var);     // NOLINT
    *varPtr = value;
}

void subtract(VM* vm) {
    if (vm->size() < 2) return;
    Value right = vm->pop();
    Value left = vm->pop();

    if (bool res = left.index() != right.index(); res) {
        switch (left.index()) {
        case INTEGER:  vm->push(Integer(asReal(left) - asReal(right) + Half));          break;
        case REAL:     vm->push(asReal(left) - asReal(right));                          break;
        case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"-", right); vm->push(left); break;
        case VALUEPTR: vm->push(left);                                                  break;
        case STRING: {
                auto s = std::get<STRING>(left);
                switch (right.index()) {
                case INTEGER:
                case REAL: {
                        auto x = asInteger(right);
                        if (x > s.size()) s = L"";
                        else s = s.substr(0, s.size() - x);
                    }
                    break;
                }
                vm->push(s);
            }
            break;
        }
        return;
    }
    switch (left.index()) {
    case INTEGER:  vm->push(std::get<INTEGER>(left) - std::get<INTEGER>(right));    break;
    case REAL:     vm->push(std::get<REAL>(left) - std::get<REAL>(right));          break;
    case EXTERNAL: std::get<EXTERNAL>(left)->send(vm, L"-", right); vm->push(left); break;
    case VALUEPTR: vm->push(left);                                                  break;
    case STRING:   {
            auto s = asString(left);
            auto r = asString(right);
            if (size_t p = s.find(r); p != std::string::npos) s = s.substr(0, p) + s.substr(p + r.size());
        }
        break;
    }
}

void then(VM* vm) {
    if (vm->compiling()) {
        Compiled* code = dynamic_cast<Compiled*>(vm->code());
        code->branch(1);
        auto loc = code->jump(0);
        vm->syspush(Integer(loc));
    }
}

void var(VM* vm) {
    if (auto val = vm->word(true); val.has_value()) {
        vm->pop();
        if (Value value = val.value(); value.index() == STRING) {
            auto name = std::get<String>(value);
            if (vm->compiling()) {
                vm->code()->locals()[name] = { 0 };
                vm->code()->reverse()[vm->code()->locals()[name].data()] = name;
                return;
            }
            vm->globals()[name] = { 0 };
            vm->reverse()[vm->globals()[name].data()] = name;
        }
    }
}

void word(VM* vm) {
    String& buffer = vm->buffer();

    while (!buffer.empty() && isspace(buffer[0])) buffer = buffer.substr(1);

    // if begins and ends with same quote: push string
    String wrd;
    if (!buffer.empty() && (buffer.front() == '"' || buffer.front() == '\'')) {
        auto quote = buffer[0];
        buffer = buffer.substr(1);
        bool escape = false;
        while (!buffer.empty() && (escape || buffer[0] != quote)) {
            if (escape) {
                escape = false;
                switch (buffer[0]) {
                case 'n':  wrd += L"\n";     break;
                case 'r':  wrd += L"\r";     break;
                case 't':  wrd += L"\t";     break;
                case '\\': wrd += L"\\";     break;
                default:   wrd += buffer[0]; break;
                }
            } else {
                if (buffer[0] == '\\') escape = true;
                else wrd += buffer[0];
            }
            buffer = buffer.substr(1);
        }
        if (buffer.empty()) vm->push(quote + wrd);
        else {
            buffer = buffer.substr(1);
            vm->push(wrd);
        }
        return;
    }
    else {
        while (!buffer.empty() && !isspace(buffer[0])) {
            wrd += buffer[0];
            buffer = buffer.substr(1);
        }
        if (wrd.empty()) return;
    }

    // if it could be anumber of some sort
    if (auto ch = wrd[0]; ch == '-' || isdigit(ch)) {
        // if it's an integer: push integer
        size_t pos = 0;
        try {
            Integer num = std::stoll(wrd, &pos);
            if (pos == wrd.size()) {
                vm->push(num);
                return;
            }
        } catch (...) { }

        // if it's a real number: push real
        pos = 0;
        try {
            Real num = std::stold(wrd, &pos);
            if (pos == wrd.size()) {
                vm->push(num);
                return;
            }
        } catch (...) { }
    }

    vm->push(wrd);
}

}

Fifth::VM::VM()
{
    builtin(L"array",   array,      IMMEDIATE);
    builtin(L"by",      by,         IMMEDIATE | COMPILETIME);
    builtin(L"dbg",     dbg,        IMMEDIATE);
    builtin(L"def",     def,        IMMEDIATE);
    builtin(L"do",      doDo,       IMMEDIATE | COMPILETIME);
    builtin(L"done",    done,       IMMEDIATE | COMPILETIME);
    builtin(L"else",    doElse,     IMMEDIATE | COMPILETIME);
    builtin(L"endif",   doEndIf,    IMMEDIATE | COMPILETIME);
    builtin(L"for",     doFor,      IMMEDIATE | COMPILETIME);
    builtin(L"if",      [](VM*){ }, IMMEDIATE | COMPILETIME);
    builtin(L"next",    next,       IMMEDIATE | COMPILETIME);
    builtin(L"each",    step,       IMMEDIATE | COMPILETIME);
    builtin(L"return",  doReturn,   IMMEDIATE | COMPILETIME);
    builtin(L"then",    then,       IMMEDIATE | COMPILETIME);
    builtin(L"while",   doWhile,    IMMEDIATE | COMPILETIME);
    builtin(L"var",     var,        IMMEDIATE);

    builtin(L"and",     [](VM* vm) { Value right = vm->pop(); Value left = vm->pop(); vm->push(isTrue(left) && isTrue(right)); });
    builtin(L"ch",      [](VM* vm) { cstd::out.putChar(asInteger(vm->pop())); });                                                                                        // NOLINT
    builtin(L"dup",     [](VM* vm) { vm->dup(); });
    builtin(L"empty",   [](VM* vm) { vm->push(vm->empty()); });
    builtin(L"explode", explode);
    builtin(L"get",     get);
    builtin(L"len",     len);
    builtin(L"move",    [](VM* vm) { vm->move(); });
    builtin(L"nand",    [](VM* vm) { Value right = vm->pop(); Value left = vm->pop(); vm->push(!(isTrue(left) && isTrue(right))); });
    builtin(L"nor",     [](VM* vm) { Value right = vm->pop(); Value left = vm->pop(); vm->push(!(isTrue(left) || isTrue(right))); });
    builtin(L"nth",     [](VM* vm) { vm->push(vm->nth((asInteger(vm->pop())))); });
    builtin(L"or",      [](VM* vm) { Value right = vm->pop(); Value left = vm->pop(); vm->push(isTrue(left) || isTrue(right)); });
    builtin(L"pop",     [](VM* vm) { vm->pop(); });
    builtin(L"print",   [](VM* vm) { cstd::out.putString(asString(vm->pop())); });
    builtin(L"rot",     [](VM* vm) { vm->rot(); });
    builtin(L"rrot",    [](VM* vm) { vm->rrot(); });
    builtin(L"size",    [](VM* vm) { vm->size(); });
    builtin(L"swap",    [](VM* vm) { vm->swap(); });
    builtin(L"sysdup",  [](VM* vm) { vm->sysdup(); });
    builtin(L"sysmove", [](VM* vm) { vm->sysmove(); });
    builtin(L"sysover", [](VM* vm) { vm->sysover(); });
    builtin(L"syspush", [](VM* vm) { vm->syspush(vm->pop()); });
    builtin(L"syspop",  [](VM* vm) { vm->syspop(); });
    builtin(L"sysswap", [](VM* vm) { Value x = vm->syspop(), y = vm->pop(); vm->syspush(y); vm->push(x); });
    builtin(L"systop",  [](VM* vm) { vm->push(vm->systop()); });
    builtin(L"vector",  [](VM* vm) { Vector* v = new Vector(); vm->push(v); });                                                                                          // NOLINT
    builtin(L"word",    Fifth::word);
    builtin(L"xor",     [](VM* vm) { Value right = vm->pop(); Value left = vm->pop(); vm->push((isTrue(left) || isTrue(right)) && !(isTrue(left) && isTrue(right))); });

    builtin(L"(",  algebra, IMMEDIATE);

    builtin(L"->", storeRight);
    builtin(L"<-", storeLeft);
    builtin(L"<>", notEqual);
    builtin(L"!=", notEqual);
    builtin(L"=",  equal);
    builtin(L"<=", lessEqual);
    builtin(L"<",  less);
    builtin(L">=", greaterEqual);
    builtin(L">",  greater);
    builtin(L"+",  add);
    builtin(L"-",  subtract);
    builtin(L"*",  multiply);
    builtin(L"/",  divide);
    builtin(L"%",  modulo);
    builtin(L"^",  power);
}

void Fifth::VM::breakAt(int at) {
    for (auto x = mBreakPoints.begin(); x != mBreakPoints.end(); ++x) {
        if (x->function == mDebug && x->pc == at) {
            mBreakPoints.erase(x);
            return;
        }
    }
    mBreakPoints.emplace_back(mDebug, at);
}

std::vector<Fifth::VM::At> Fifth::VM::breakPoints(Code* in)
{
    std::vector<Fifth::VM::At> points;
    for (const auto& x: mBreakPoints) if (x.function == in) points.push_back(x);
    return points;
}

void Fifth::VM::clearStack()
{
    mDebugStack.clear();
}

std::vector<std::wstring> Fifth::VM::debug(const std::wstring& name) {
    std::vector<std::wstring> code;
    mDebug = nullptr;
    if (!mDictionary.contains(name)) return code;

    mDebug = mDictionary[name];
    mPC = 0;
    if (auto* block = dynamic_cast<Compiled*>(mDebug); block) {
        size_t sz = block->size();
        for (size_t i = 0; i < sz; ++i) {
            String line = std::to_wstring(i) + L",";
            const auto& instr = block->get(i);
            switch (instr.op()) {
            case Compiled::NOP:     line += L"NOP";                                             break;
            case Compiled::POP:     line += L"POP";                                             break;
            case Compiled::SYSPOP:  line += L"SYSPOP";                                          break;
            case Compiled::RETURN:  line += L"RETURN";                                          break;
            case Compiled::PUSH:    line += L"PUSH," + toString(this, block, instr.value());    break;
            case Compiled::SYSPUSH: line += L"SYSPUSH," + toString(this, block, instr.value()); break;
            case Compiled::JUMP:    line += L"JUMP," + asString(instr.by());                    break;
            case Compiled::BRANCH:  line += L"BRANCH," + asString(instr.by());                  break;
            case Compiled::CALL:
                auto* code = instr.code();
                String out = L"<unknown>";
                if (String nm = nameOf(code); !nm.empty()) out = nm;
                line += L"CALL," + out;
                break;
            }
            code.push_back(line);
        }
    } else code.push_back(name + L",builtin");

    return code;
}

bool Fifth::VM::execute(const std::wstring& s) {
    buffer(s);

    bool first = true;
    for ( ; ; ) {
        auto val = word(first);
        first = false;;
        if (!val.has_value()) break;

        Value value = val.value();
        if (value.index() == STRING) {
            String word = std::get<String>(value);
            if (mDictionary.contains(word)) {
                pop();
                Code* code = mDictionary[word];
                if (code->compileTime()) continue;
                code->exec(this);
            } else if (mGlobals.contains(word)) {
                pop();
                push((void*) mGlobals[word].data());
            }
        }
    }
    return true;
}

std::vector<std::wstring> Fifth::VM::getCompiled()
{
    std::vector<std::wstring> names;
    for (const auto item: mDictionary) {
        if (const auto compiled = dynamic_cast<Compiled*>(item.second); compiled) names.push_back(item.first);
    }
    return names;
}

std::vector<std::wstring> Fifth::VM::globalVars()
{
    std::vector<std::wstring> vars;
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    for (auto& x: globals()) {
        Value val = *(Value*) x.second.data();
        vars.push_back(x.first + L"," + toString(this, code, val));
    }
    return vars;
}

std::vector<std::wstring> Fifth::VM::localVars()
{
    std::vector<std::wstring> vars;
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    for (auto& x: mDebug->locals()) {
        Value val = *(Value*) x.second.data();
        vars.push_back(x.first + L"," + toString(this, code, val));
    }
    return vars;
}

size_t Fifth::VM::pc() {
    return mPC;
}

void Fifth::VM::run() {
    for (; ; ) {
        stepInto();
        if (mDebug == nullptr) break;
        for (const auto& x: mBreakPoints) if (x.function == mDebug && mPC == x.pc) break;
    }
}

void Fifth::VM::stepInto() {
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    if (code->get(mPC).op() != Compiled::CALL) stepOver();
    const auto& instr = code->get(mPC);
    String name = nameOf(instr.code());
    if (!name.empty() && dynamic_cast<Compiled*>(instr.code()) != nullptr) {
        mDebugStack.emplace_back(mDebug, mPC);
        debug(name);
    } else stepOver();
}

void Fifth::VM::stepOver() {
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    switch (code->get(mPC).op()) {
    case Compiled::NOP:                                                    break;
    case Compiled::PUSH:    push(code->get(mPC).value());                  break;
    case Compiled::SYSPUSH: syspush(code->get(mPC).value());               break;
    case Compiled::POP:     pop();                                         break;
    case Compiled::SYSPOP:  syspop();                                      break;
    case Compiled::CALL:    code->get(mPC).code()->exec(this);             break;
    case Compiled::JUMP:    mPC += code->get(mPC).by();                    break;
    case Compiled::BRANCH:  if (isTrue(pop())) mPC += code->get(mPC).by(); break;
    case Compiled::RETURN:
        if (mDebugStack.empty()) {
            mDebug = nullptr;
            return;
        }
        mDebug = mDebugStack.back().function;
        mPC = mDebugStack.back().pc;
        break;
    }
    ++mPC;
}

std::vector<std::wstring> Fifth::VM::user() {
    std::vector<std::wstring> values;
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    for (auto& x: mUser) values.push_back(toString(this, code, x));
    return values;
}

std::vector<std::wstring> Fifth::VM::system() {
    std::vector<std::wstring> values;
    Compiled* code = dynamic_cast<Compiled*>(mDebug);
    for (auto& x: mSystem) values.push_back(toString(this, code, x));
    return values;
}

std::optional<Fifth::Value> Fifth::VM::word(bool reload) {
    static Code* word = nullptr;                                   // NOLINT
    if (reload || word == nullptr) word = mDictionary[L"word"];

    auto test = size();
    word->exec(this);
    if (test == size()) return std::optional<Value>();

    return top();
}

std::wstring Fifth::VM::debugUserStack() {
    std::wstring res;
    while (size()) {
        res = asString(top()) + (res.size() ? L" " + res : L"");
        pop();
    }
    return res;
}

void Fifth::Compiled::exec(VM* vm) {
    for (size_t pc = 0; ; ++pc) {
        switch (mBlock[pc].op()) {
        case NOP:                                                   break;
        case PUSH:    vm->push(mBlock[pc].value());                 break;
        case SYSPUSH: vm->syspush(mBlock[pc].value());              break;
        case POP:     vm->pop();                                    break;
        case SYSPOP:  vm->syspop();                                 break;
        case CALL:    mBlock[pc].code()->exec(vm);                  break;
        case JUMP:    pc += mBlock[pc].by();                        break;
        case BRANCH:  if (isTrue(vm->pop())) pc += mBlock[pc].by(); break;
        case RETURN:                                                return;
        }
    }
}
