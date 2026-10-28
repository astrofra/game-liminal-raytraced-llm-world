configure_file("${CMAKE_CURRENT_LIST_DIR}/xgrammar-config.cmake"
	"${XGRAMMAR_SOURCE_DIR}/config.cmake" COPYONLY)
# libc++ C++20 requires NamedGrammar to be complete before constructing its vector.
set(header "${XGRAMMAR_SOURCE_DIR}/include/xgrammar/grammar.h")
file(READ "${header}" content)
if(content MATCHES "EmptyNamedGrammars")
	return()
endif()
string(REPLACE "struct NamedGrammar;" "struct NamedGrammar;\nconst std::vector<NamedGrammar>& EmptyNamedGrammars();" content "${content}")
string(REPLACE "const std::vector<NamedGrammar>& named_grammars = {}"
	"const std::vector<NamedGrammar>& named_grammars = EmptyNamedGrammars()" content "${content}")
string(REPLACE "}  // namespace xgrammar"
	"inline const std::vector<NamedGrammar>& EmptyNamedGrammars() {\n  static const std::vector<NamedGrammar> empty;\n  return empty;\n}\n\n}  // namespace xgrammar" content "${content}")
file(WRITE "${header}" "${content}")
