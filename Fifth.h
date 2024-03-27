#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Fifth {

typedef    long long Integer;
typedef  long double Real;
typedef std::wstring String;
class Map;
class Stack;
class Vector;
class External;

typedef std::variant<Integer, Real, String, External*, void*> Value;
enum Type { INTEGER, REAL, STRING, EXTERNAL, VALUEPTR };

static constexpr  int   IMMEDIATE = 0b00000001;
static constexpr  int COMPILETIME = 0b00000010;
static constexpr bool RELOAD      = true;

class Map {
private:
    std::unordered_map<Value, Value> mValue;

public:
    Map() {}

    Value operator[](const Value& x) { return mValue[x]; }

      Map* append(const Map* m) { mValue.insert(m->mValue.begin(), m->mValue.end()); return this; }
      void clear()              { mValue.clear(); }
      bool contains(Value x )   { return mValue.contains(x); }
      void erase(Value x)       { if (mValue.contains(x)) { mValue.erase(mValue.find(x)); } }
    size_t size() const         { return mValue.size(); }
      bool empty()              { return mValue.empty(); }
};

class Vector {
private:
    std::vector<Value> mValue;

public:
    Vector() { }

    Value operator[](Integer x) { return mValue[x]; }

       void append(Value v)         { mValue.push_back(v); }
    Vector* append(const Vector* v) { mValue.insert(mValue.end(), v->mValue.begin(), v->mValue.end()); return this; }
       void clear()                 { mValue.clear(); }
       bool empty()                 { return mValue.empty(); }
       void pop()                   { mValue.pop_back(); }
       void push_back(Value v)      { mValue.push_back(v); }
     size_t size()                  { return mValue.size(); }
};

class VM;

#ifndef NO
#define NO(x) x(const x&) = delete; x(x&&) = delete; x& operator=(const x&) = delete; x& operator=(x&&) = delete; // NOLINT
#endif

class External {
public:
    External() { }
    virtual ~External() { }

    NO(External);

    virtual bool empty()                                      { return true; }
    virtual void install(VM*)                                 { } // <-- handles installing this External type words.
    virtual void send(VM*, const std::wstring&, const Value&) { }
};

inline Integer asInteger(const Value& v) {
    switch (v.index()) {
    case INTEGER:  return std::get<Integer>(v);
    case REAL:     return std::get<Real>(v);                       // NOLINT
    case STRING:   return std::stoll(std::get<String>(v));         // NOLINT
    case EXTERNAL: return 0;
    case VALUEPTR: return asInteger(*(Value*) std::get<void*>(v)); // NOLINT
    }
    return 0;
}

inline Real asReal(const Value& v) {
    switch (v.index()) {
    case INTEGER:  return std::get<Integer>(v);
    case REAL:     return std::get<Real>(v);                       // NOLINT
    case STRING:   return std::stold(std::get<String>(v));         // NOLINT
    case EXTERNAL: return 0;
    case VALUEPTR: return asReal(*(Value*) std::get<void*>(v));    // NOLINT
    }
    return 0;
}

inline std::wstring asString(const Value& v) {
    switch (v.index()) {
    case INTEGER:  return std::to_wstring(std::get<Integer>(v));
    case REAL:     return std::to_wstring(std::get<Real>(v));
    case STRING:   return std::get<String>(v);
    case EXTERNAL: return L"(x)";
    case VALUEPTR: return asString(*(Value*) std::get<void*>(v)); // NOLINT
    }
    return L"";
}

inline bool isTrue(const Value& v) {
    switch (v.index()) {
    case INTEGER:  return std::get<Integer>(v) != 0;
    case REAL:     return std::get<Real>(v) != 0.0;
    case STRING:   return !std::get<String>(v).empty();
    case EXTERNAL: return !std::get<External*>(v)->empty();
    case VALUEPTR: return isTrue(*(Value*) std::get<void*>(v)); // NOLINT
    }
    return false;
}

class Stack {
private:
    std::vector<Value> mValue;

public:
    Stack() { }

    auto begin() { return mValue.begin(); }
    auto end()   { return mValue.end(); }

    Stack* append(Stack* s)     { mValue.insert(mValue.end(), s->mValue.begin(), s->mValue.end()); return this; }
      void dup()                { nth(0); }
      bool empty()              { return mValue.empty(); }
      bool isEmpty()            { return empty(); }
      void nth(Integer n)       { push(mValue[size() - (n + 1)]); }
      void over()               { nth(1); }
     Value pop()                { Value v = mValue.back(); mValue.pop_back(); return v; }
      void push(const Value& v) { mValue.push_back(v); }
      void rot()                { Value a = pop();  Value b = pop(); Value c = pop(); push(b); push(a); push(c); }
      void rrot()               { Value a = pop();  Value b = pop(); Value c = pop(); push(a); push(c); push(b); }
    size_t size()               { return mValue.size(); }
      void swap()               { Value a = pop(); Value b = pop(); push(a); push(b); } // NOLINT
     Value top()                { return mValue.back(); }
};

class Code {
private:
                                     int mFlags;
    std::map<String, std::vector<Value>> mLocals;
                std::map<Value*, String> mReverse;

public:
    Code(int flags = 0)
        : mFlags(flags)    { }
    virtual ~Code() { }

    NO(Code);

     bool compileTime() const   { return mFlags & COMPILETIME; }
     bool immediate() const     { return mFlags & IMMEDIATE; }
     bool isCompileTime() const { return compileTime(); }
     bool isImmediate() const   { return immediate(); }
    auto& locals()              { return mLocals; }
    auto& reverse()             { return mReverse; }

    virtual void exec(VM*) = 0;
};

typedef std::function<void(VM*)> Lambda;

class Builtin: public Code {
private:
    Lambda mFunction;

public:
    Builtin(const Lambda& function, int flags = 0)
        : Code(flags)
        , mFunction(function)
    { }

    void exec(VM* vm) override { mFunction(vm); }
};

class Compiled: public Code {
public:
    enum opCode { NOP, PUSH, SYSPUSH, POP, SYSPOP, CALL, JUMP, BRANCH, RETURN };
    class Instruction {
        opCode mOpCode;
        std::variant<std::nullptr_t, int, Code*, Value> mArgument;

    public:
        Instruction(opCode op)
            : mOpCode(op)
            , mArgument(nullptr)
        { }
        Instruction(opCode op, int by)
            : mOpCode(op)
            , mArgument(by)
        { }
        Instruction(opCode op, const Value& value)
            : mOpCode(op)
            , mArgument(value)
        { }
        Instruction(opCode op, Code* value)
            : mOpCode(op)
            , mArgument(dynamic_cast<Code*>(value))
        { }

        int    by() const    { return std::get<int>(mArgument); }
        Code*  code() const  { return std::get<Code*>(mArgument); }
        opCode op() const    { return mOpCode; }
        void   setBy(int b)  { mArgument = b; }
        Value  value() const { return std::get<Value>(mArgument); }
    };

private:
    std::vector<Instruction> mBlock;

public:
    Compiled()
        : Code()
    { }

    void exec(VM* vm) override;

                size_t branch(int x)           { mBlock.emplace_back(BRANCH, x);  return location(); }
                size_t call(Code* c)           { mBlock.emplace_back(CALL, c);    return location(); }
    const Instruction& get(size_t x)           { return mBlock[x]; }
                size_t jump(int x)             { mBlock.emplace_back(JUMP, x);    return location(); }
                size_t location()              { return size() - 1; }
                size_t pop()                   { mBlock.emplace_back(POP);        return location(); }
                size_t push(const Value& v)    { mBlock.emplace_back(PUSH, v);    return location(); }
                size_t ret()                   { mBlock.emplace_back(RETURN);     return location(); }
                size_t size()                  { return mBlock.size(); }
                size_t syspop()                { mBlock.emplace_back(SYSPOP);     return location(); }
                size_t syspush(const Value& v) { mBlock.emplace_back(SYSPUSH, v); return location(); }
                  void update(int loc, int by) { mBlock[loc].setBy(by); }
};

class VM {
private:
    struct At {
         Code* function;
        size_t pc;

        At(Code* f, size_t p)
            : function(f)
            , pc(p)
        {}
    };

                         std::vector<At> mBreakPoints;
                         std::vector<At> mDebugStack;
                                  String mBuffer;
                                   Code* mCode = nullptr;
                                    bool mCompiling = false;
                                   Code* mDebug = nullptr;
                 std::map<String, Code*> mDictionary;
                 std::map<Code*, String> mNameOf;
    std::map<String, std::vector<Value>> mGlobals;
                                  size_t mPC = 0;;
                std::map<Value*, String> mReverse;
                                     int mSkipping = 0;
                                   Stack mSystem;
                                   Stack mUser;

public:
    VM();

    String& buffer()                         { return mBuffer; }
    String& buffer(const String& s)          { mBuffer = s; return mBuffer; }
       void builtin(const String& x,
                    const Lambda& l,
                    int flags = 0)           { mDictionary[x] = new Builtin(l, flags); nameOf(mDictionary[x], x); }                         // NOLINT
      Code* code()                           { return mCode; }
      Code* code(Code* c)                    { mCode = c; return code(); }
       bool compiling()                      { return mCompiling; }
       bool compiling(bool c)                { mCompiling = c; return compiling(); }
     String debugging()                      { return nameOf(mDebug); }
      auto& dictionary()                     { return mDictionary; }
       void dup()                            { mUser.dup(); }
       bool empty()                          { return mUser.empty(); }
      auto& globals()                        { return mGlobals; }
       void install(External* x)             { x->install(this); }
       bool isCompiling()                    { return compiling(); }
       void move()                           { mUser.push(mSystem.pop()); }
     String nameOf(Code* c)                  { return mNameOf.contains(c) ? mNameOf[c] : L""; }
     String nameOf(Code* c, const String& s) { mNameOf[c] = s; return nameOf(c); }
      Value nth(size_t n)                    { mUser.nth((Integer) n); return pop(); }
       void over()                           { mUser.over(); }
      Value pop()                            { return mUser.pop(); }
       void push(const Value& v)             { mUser.push(v); }
      auto& reverse()                        { return mReverse; }
       void rot()                            { mUser.rot(); }
       void rrot()                           { mUser.rrot(); }
       bool skipping()                       { return mSkipping != 0; }
       bool skipping(bool s)                 { if (s) ++mSkipping; else --mSkipping; return skipping(); }
     size_t size()                           { return mUser.size(); }
       void swap()                           { mUser.swap(); }                                                                          // NOLINT
       void sysdup()                         { mSystem.push(mSystem.top()); }
       void sysmove()                        { mSystem.push(mUser.pop()); }
       void sysover()                        { mSystem.over(); }
      Value syspop()                         { return mSystem.pop(); }
       void syspush(const Value& v)          { mSystem.push(v); }
      Value systop()                         { return mSystem.top(); }
      Value top()                            { return mUser.top(); }

                         void breakAt(int at);
              std::vector<At> breakPoints(Code* in);
                         void clearStack();
    std::vector<std::wstring> debug(const std::wstring& name);
                         bool execute(const std::wstring& s);
    std::vector<std::wstring> getCompiled();
    std::vector<std::wstring> globalVars();
    std::vector<std::wstring> localVars();
                       size_t pc();
                         void run();
                         void stepInto();
                         void stepOver();
    std::vector<std::wstring> user();
    std::vector<std::wstring> system();
         std::optional<Value> word(bool reload = false);

    std::wstring debugUserStack();
};

}
