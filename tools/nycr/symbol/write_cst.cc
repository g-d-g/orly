/* <tools/nycr/symbol/write_cst.cc>

   Implements <tools/nycr/symbol/write_cst.h>.

   Copyright 2010-2014 OrlyAtomics, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <tools/nycr/symbol/write_cst.h>

#include <cassert>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

#include <tools/nycr/symbol/base.h>
#include <tools/nycr/symbol/for_each_final.h>
#include <tools/nycr/symbol/for_each_known_kind.h>
#include <tools/nycr/symbol/keyword.h>
#include <tools/nycr/symbol/operator.h>
#include <tools/nycr/symbol/output_file.h>
#include <tools/nycr/symbol/rule.h>
#include <tools/nycr/symbol/write_nycr.h>

using namespace std;
using namespace placeholders;
using namespace Tools::Nycr::Symbol;

static void WriteCstH(const char *root, const char *branch, const char *atom, const TLanguage *language);

static void WriteCstCc(const char *root, const char *branch, const char *atom, const TLanguage *language);

static void WriteDecl(const TKind *kind, const string &namespace_prefix, ostream &strm);

static void WriteDef(const TKind *kind, ostream &strm);

static void WriteFwdDecl(const TKind *kind, ostream &strm);

void Tools::Nycr::Symbol::WriteCst(const char *root, const char *branch, const char *atom, const TLanguage *language) {
  WriteCstH(root, branch, atom, language);
  WriteCstCc(root, branch, atom, language);
}

static void WriteCstH(const char *root, const char *branch, const char *atom, const TLanguage *language) {
  assert(language);
  ofstream strm;
  CreateOutputFile(root, branch, atom, language, ".cst.h", strm);
  strm
      << "#pragma once" << endl << endl
      << "#include <cassert>" << endl
      << "#include <ostream>" << endl << endl
      << "#include <base/class_traits.h>" << endl
      << "#include <tools/nycr/context.h>" << endl
      << "#include <tools/nycr/indent.h>" << endl
      << "#include <tools/nycr/lexeme.h>" << endl
      << "#include <tools/nycr/test.h>" << endl << endl;
  ostringstream namespace_prefix_builder;
  const TLanguage::TNamespaces &namespaces = language->GetNamespaces();
  if (!namespaces.empty()) {
    for (auto iter = namespaces.begin(); iter != namespaces.end(); ++iter) {
      auto upper_name = TUpper(*iter);
      strm << "namespace " << upper_name << " {" << endl;
      namespace_prefix_builder << "::" << upper_name;
    }
    namespace_prefix_builder << "::";
    strm << endl;
  }

  strm << endl
       << "struct yy_extra_t { Tools::Nycr::TContext*ctx; int depth; int line; int column; };" << endl
       << endl;
  const string namespace_prefix = namespace_prefix_builder.str();
  ForEachKnownKind(language, bind(WriteFwdDecl, _1, ref(strm)));
  ForEachKnownKind(language, bind(WriteDecl, _1, cref(namespace_prefix), ref(strm)));
  if (!namespaces.empty()) {
    strm << endl;
    for (auto iter = namespaces.rbegin(); iter != namespaces.rend(); ++iter) {
      strm << "}  // " << TUpper(*iter) << endl;
    }
  }
}

static void WriteCstCc(const char *root, const char *branch, const char *atom, const TLanguage *language) {
  assert(language);
  ofstream strm;
  CreateOutputFile(root, branch, atom, language, ".cst.cc", strm);
  strm
      << "#include <" << TPath(branch, atom, language) << ".cst.h>" << endl << endl
      << "#include <cstdio>" << endl << endl
      << "#include <base/class_traits.h>" << endl
      << "#include <base/thrower.h>" << endl
      << "#include <tools/nycr/test.h>" << endl
      << "#include <" << TPath(branch, atom, language) << ".bison.h>" << endl
      << "#include <" << TPath(branch, atom, language) << ".flex.h>" << endl << endl
      << "#include <iostream>" << endl
      << "using namespace std;" << endl
      << TUsingNamespace(language)
      << "extern void *" << TUnderscore(language) << "_scan_string (const char *yy_str ,void *yyscanner );" << endl
      << "extern void " << TUnderscore(language) << "parse(void *scanner, Tools::Nycr::TContextBuilt<" << TType(language->GetName()) << "> &cst_out);" << endl << endl
      << "extern void " << TUnderscore(language) << "set_in(FILE *in_str ,void *yyscanner );" << endl
      << "extern int " << TUnderscore(language) << "lex_init(void **ptr_yy_globals);" << endl
      << "extern int " << TUnderscore(language) << "lex_destroy(void *ptr_yy_globals);" << endl
      << "extern void " << TUnderscore(language) << "set_extra(yy_extra_t, void *yyscanner);" << endl
      << endl
      << "Tools::Nycr::TContextBuilt<" << TType(language->GetName()) << "> " << TType(language->GetName()) << "::ParseFile(const char *path) {" << endl
      << "  Tools::Nycr::TContextBuilt<" << TType(language->GetName()) << "> ctx;" << endl
      << "  void *scanner;" << endl
      << "  " << TUnderscore(language) << "lex_init(&scanner);" << endl
      << "  " << TUnderscore(language) << "set_extra({&ctx, 0, 1, 1}, scanner);" << endl
      << "  FILE *in_file = fopen(path, \"r\");" << endl
      << "  if (!in_file) {" << endl
      << "    THROW << \"could not open \\\"\" << path << '\\\"';" << endl
      << "  }" << endl
      << "  " << TUnderscore(language) << "set_in(in_file, scanner);" << endl
      << "  " << TUnderscore(language) << "parse(scanner, ctx);" << endl
      << "  " << TUnderscore(language) << "lex_destroy(scanner);" << endl
      << "  fclose(in_file);" << endl
      << "  ctx.SortErrors();" << endl
      << "  return ctx;" << endl
      << '}' << endl
      << endl
      << "Tools::Nycr::TContextBuilt<" << TType(language->GetName()) << "> " << TType(language->GetName()) << "::ParseStr(const char *str) {" << endl
      << "  Tools::Nycr::TContextBuilt<" << TType(language->GetName()) << "> ctx;" << endl
      << "  void *scanner;"
      << "  " << TUnderscore(language) << "lex_init(&scanner);" << endl
      << "  " << TUnderscore(language) << "set_extra({&ctx, 0, 1, 1}, scanner);" << endl
      << "  " << TUnderscore(language) << "_scan_string(str, scanner);" << endl
      << "  " << TUnderscore(language) << "parse(scanner, ctx);" << endl
      << "  " << TUnderscore(language) << "lex_destroy(scanner);" << endl
      << "  ctx.SortErrors();" << endl
      << "  return ctx;" << endl
      << '}' << endl;
  ForEachKnownKind(language, bind(WriteDef, _1, ref(strm)));
}

static void WriteDecl(const TKind *kind, const string &namespace_prefix, ostream &strm) {
  class TVisitor : public TKind::TVisitor {
    public:
    TVisitor(ostream &strm, const string &namespace_prefix) : Strm(strm), NamespacePrefix(namespace_prefix) {}
    virtual void operator()(const TBase *that) const {
      WriteOpen(that);
      Strm << "  class TVisitor { " << endl
           << "    public:" << endl
           << "      virtual ~TVisitor() {}" << endl;
      ForEachFinal(that, [&](const TFinal *that) {
        Strm << "      virtual void operator()(const " << NamespacePrefix
             << TType(that->GetName()) << " *that) const = 0;" << endl;
      });
      Strm << "    protected:" << endl
           << "      TVisitor() {}" << endl
           << "    };  // TVisitor" << endl
           << "  virtual ~" << TType(that->GetName()) << "() {}" << endl;

      for(const TBase *base = static_cast<const TKind*>(that)->GetBase(); base; base = static_cast<const TKind*>(base)->GetBase()) {
        Strm << "  using " << TType(base->GetName()) << "::Accept;" << endl;
      }


      Strm << "  virtual void Accept(const TVisitor &visitor) const = 0;" << endl
           << "  virtual void Write(std::ostream &strm, size_t depth, const char *as_member) const = 0;" << endl
           << "  virtual bool Test(::Tools::Nycr::Test::TNode *that, const char *as_member) const = 0;" << endl
           << "  protected:" << endl
           << "  " << TType(that->GetName()) << "() {}" << endl;
      WriteClose(that);
    }
    virtual void operator()(const TLanguage *that) const {
      WriteOpen(that);
      OnCompound(that, true);
      WriteClose(that);
    }
    virtual void operator()(const TOperator *that) const {
      OnAtom(that);
    }
    virtual void operator()(const TKeyword *that) const {
      OnAtom(that);
    }
    virtual void operator()(const TRule *that) const {
      WriteOpen(that);
      OnCompound(that, false);
      WriteClose(that);
    }
    private:
    void OnAtom(const TAtom *that) const {
      WriteOpen(that);
      Strm << "  " << TType(that->GetName())
           << "(int start_line, int start_col, int limit_line, int limit_col, const char *text, int len)" << endl
           << "      : Lexeme(start_line, start_col, limit_line, limit_col, text, len) {}" << endl;
      WriteAccepts(that);
      Strm << "  const ::Tools::Nycr::TLexeme &GetLexeme() const {" << endl
           << "    assert(this);" << endl
           << "    return Lexeme;" << endl
           << "  }" << endl
           << "  virtual void Write(std::ostream &strm, size_t depth, const char *as_member) const;" << endl
           << "  virtual bool Test(::Tools::Nycr::Test::TNode *that, const char *as_member) const;" << endl
           << "  private:" << endl
           << "  ::Tools::Nycr::TLexeme Lexeme;" << endl;
      WriteClose(that);
    }
    void OnCompound(const TCompound *that, bool is_language) const {
      const TCompound::TMembersInOrder &members_in_order = that->GetMembersInOrder();
      Strm << "  " << TType(that->GetName()) << '(';
      bool sep = false;
      for (auto iter = members_in_order.begin();
           iter != members_in_order.end(); ++iter) {
        const TCompound::TMember *member = *iter;
        const TKind *kind = member->TryGetKind();
        if (kind) {
          if (sep) {
            Strm << ", ";
          } else {
            sep = true;
          }
          Strm << "std::unique_ptr<" << TType(kind->GetName()) << "> &&" << TLower(member->GetName());
        }
      }
      Strm << ')';
      if (members_in_order.empty()) {
        Strm << " {}" << endl;
      } else {
        Strm << endl << "      : ";
        sep = false;
        for (auto iter = members_in_order.begin();
             iter != members_in_order.end(); ++iter) {
          const TCompound::TMember *member = *iter;
          const TKind *kind = member->TryGetKind();
          if (kind) {
            if (sep) {
              Strm << ", ";
            } else {
              sep = true;
            }
            const TName &name = member->GetName();
            Strm << TUpper(name) << "(std::move(" << TLower(name) << "))";
          }
        }
        Strm << " {" << endl;
        for (auto iter = members_in_order.begin();
             iter != members_in_order.end(); ++iter) {
          const TCompound::TMember *member = *iter;
          if (member->TryGetKind()) {
            Strm << "    assert(" << TUpper(member->GetName()) << ");" << endl;
          }
        }
        Strm << "  }" << endl
             << "  virtual ~" << TType(that->GetName()) << "();" << endl;
      }
      WriteAccepts(that);
      for (auto iter = members_in_order.begin();
           iter != members_in_order.end(); ++iter) {
        const TCompound::TMember *member = *iter;
        const TKind *kind = member->TryGetKind();
        if (kind) {
          Strm
              << "  const " << TType(kind->GetName())
              << " *Get" << TUpper(member->GetName()) << "() const {" << endl
              << "    assert(this);" << endl
              << "    return " << TUpper(member->GetName()) << ".get();" << endl
              << "  }" << endl;
        }
      }
      Strm << "  virtual void Write(std::ostream &strm, size_t depth, const char *as_member) const;" << endl;
      Strm << "  virtual bool Test(::Tools::Nycr::Test::TNode *that, const char *as_member) const;" << endl;
      if (is_language) {
        Strm << "  static ::Tools::Nycr::TContextBuilt<" << TType(that->GetName()) << "> ParseFile(const char *path);" << endl;
        Strm << "  static ::Tools::Nycr::TContextBuilt<" << TType(that->GetName()) << "> ParseStr(const char *str);" << endl;
      }
      if (!members_in_order.empty()) {
        Strm << "  private:" << endl;
        for (auto iter = members_in_order.begin();
             iter != members_in_order.end(); ++iter) {
          const TCompound::TMember *member = *iter;
          const TKind *kind = member->TryGetKind();
          if (kind) {
            Strm << "  std::unique_ptr<" << TType(kind->GetName()) << "> " << TUpper(member->GetName()) << ';' << endl;
          }
        }
      }
    }
    void WriteAccepts(const TKind *that) const {
      for (;;) {
        const TBase *base = that->GetBase();
        if (!base) {
          break;
        }
        Strm << "  virtual void Accept(const " << TType(base->GetName())
             << "::TVisitor &visitor) const {" << endl
             << "    assert(this);" << endl
             << "    assert(&visitor);" << endl
             << "    visitor(this);" << endl
             << "  }" << endl;
        that = base;
      }
    }
    void WriteClose(const TKind *that) const {
      Strm << "};  // " << TType(that->GetName()) << endl;
    }
    void WriteOpen(const TKind *that) const {
      Strm << endl << "// ";
      WriteNycrDecl(that, Strm);
      Strm << "class " << TType(that->GetName());
      if (that->GetBase()) {
        Strm << " : public " << TType(that->GetBase()->GetName());
      }
      Strm << " {" << endl
           << "  NO_COPY(" << TType(that->GetName()) << ");" << endl
           << "  public:" << endl;
    }
    ostream &Strm;
    const string &NamespacePrefix;
  };
  assert(kind);
  assert(&strm);
  kind->Accept(TVisitor(strm, namespace_prefix));
}

static void WriteDef(const TKind *kind, ostream &strm) {
  assert(kind);
  assert(&strm);
  class TVisitor : public TKind::TVisitor {
    public:
    TVisitor(ostream &strm) : Strm(strm) {}
    virtual void operator()(const TBase *) const {}
    virtual void operator()(const TLanguage *that) const { OnCompound(that); }
    virtual void operator()(const TOperator *that) const { OnAtom(that); }
    virtual void operator()(const TKeyword *that) const { OnAtom(that); }
    virtual void operator()(const TRule *that) const { OnCompound(that); }
    private:
    void OnAtom(const TAtom *that) const {
      WriteWriteStart(that);
      Strm << " << ' ' << Lexeme << endl;" << endl;
      WriteWriteFinish();
      WriteTestStart(that);
      WriteTestFinish();
    }
    void OnCompound(const TCompound *that) const {
      const TCompound::TMembersInOrder &members_in_order = that->GetMembersInOrder();
      if (!members_in_order.empty()) {
        Strm
            << endl
            << TType(that->GetName()) << "::~" << TType(that->GetName()) << "() = default;" << endl
            << endl;
      }
      WriteWriteStart(that);
      Strm << " << endl;" << endl;
      if (!members_in_order.empty()) {
        Strm << "  ++depth;" << endl;
        for (auto iter = members_in_order.begin();
             iter != members_in_order.end(); ++iter) {
          const TCompound::TMember *member = *iter;
          if (member->TryGetKind()) {
            Strm << "  " << TUpper(member->GetName()) << "->Write(strm, depth, \"" << TUpper(member->GetName()) << "\");" << endl;
          }
        }
      }
      WriteWriteFinish();
      WriteTestStart(that);
      if (!members_in_order.empty()) {
        int child_idx = 0;
        for (auto iter = members_in_order.begin();
             iter != members_in_order.end(); ++iter) {
          const TCompound::TMember *member = *iter;
          if (member->TryGetKind()) {
            Strm << "  ::Tools::Nycr::Test::TNode *Child" << child_idx << " = ";
            if (child_idx == 0) {
              Strm << "that->TryGetFirstChild();" << endl;
            } else {
              Strm << "Child" << child_idx - 1 << "->TryGetNextSibling();" << endl;
            }
            Strm << "  if (Child" << child_idx << ") {" << endl
                 << "    " << TUpper(member->GetName()) << "->Test(Child" << child_idx << ", \"" << TUpper(member->GetName()) << "\");" << endl
                 << "  } else {" << endl
                 << "    cout << \"Missing Child: The concrete syntax tree has a " << TUpper(member->GetName()) << " here\" << endl;" << endl
                 << "    return false;" << endl
                 << "  }" << endl;
            ++child_idx;
          }
        }
      }
      WriteTestFinish();
    }
    void WriteWriteStart(const TKind *that) const {
      Strm
          << endl
          << "void " << TType(that->GetName()) << "::Write(ostream &strm, size_t depth, const char *as_member) const {" << endl
          << "  assert(this);" << endl
          << "  assert(&strm);" << endl
          << "  strm << ::Tools::Nycr::TIndent(depth);" << endl
          << "  if (as_member) {" << endl
          << "    strm << as_member << \" -> \";" << endl
          << "  }" << endl
          << "  strm << \"" << TUpper(that->GetName()) << '"';
    }
    void WriteWriteFinish() const {
      Strm << '}' << endl;
    }
    void WriteTestStart(const TKind *that) const {
      Strm << endl
           << "bool " << TType(that->GetName()) << "::Test(::Tools::Nycr::Test::TNode *that, const char *as_member) const {" << endl
           << "  assert(this);" << endl
           << "  if (as_member && as_member != that->GetKind()) {" << endl
           << "    cout << as_member << \" != \" << that->GetKind() << endl;" << endl
           << "    return false;" << endl
           << "  }" << endl
           << "  if (\"" << TUpper(that->GetName()) << "\" != that->GetName()) {" << endl
           << "    cout << \"" << TUpper(that->GetName()) << " != \" << that->GetName() << endl;" << endl
           << "    return false;" << endl
           << "  }" << endl;
    }
    void WriteTestFinish() const {
      Strm << "  return true;" << endl
           << '}' << endl;
    }
    ostream &Strm;
  };
  kind->Accept(TVisitor(strm));
}

static void WriteFwdDecl(const TKind *kind, ostream &strm) {
  assert(kind);
  assert(&strm);
  strm << "class " << TType(kind->GetName()) << ';' << endl;
}