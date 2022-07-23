#define UNICODE true
#define TokenType TokenType_
#include <Windows.h>
#include <ShlObj_core.h>
#include <strsafe.h>
#undef TokenType
#include <stdio.h>
#include "unified.h"
#undef ASSERT
#define ASSERT(EXPRESSION)\
do\
{\
	if (!(EXPRESSION))\
	{\
		printf(__FILE__ " (" MACRO_STRINGIFY(__LINE__) ") :: Internal error. Assertion fired : `" #EXPRESSION "`\n");\
		DEBUG_HALT();\
		abort();\
	}\
}\
while (false)

struct StringNode
{
	StringNode* next;
	String      str;
};

struct FilePathsInResult
{
	bool32      success;
	StringNode* value;
};

procedure FilePathsInResult file_paths_in(String dir_path, MemoryArena* arena)
{
	wchar_t wide_dir_path[MAX_PATH];
	u64     wide_dir_path_count;
	if (mbstowcs_s(&wide_dir_path_count, wide_dir_path, dir_path.data, static_cast<size_t>(dir_path.size)) || static_cast<i64>(wide_dir_path_count) != dir_path.size + 1)
	{
		return {};
	}

	if (StringCchCatW(wide_dir_path, capacityof(wide_dir_path), L"/*") != S_OK)
	{
		return {};
	}

	WIN32_FIND_DATAW find_data;
	HANDLE           handle = FindFirstFileW(wide_dir_path, &find_data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return {};
	}
	DEFER { FindClose(handle); };

	wide_dir_path[wide_dir_path_count] = L'\0';

	StringNode* file_names = 0;

	while (true)
	{
		BOOL status = FindNextFileW(handle, &find_data);
		if (status)
		{
			if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				u64      file_name_length = wcslen(find_data.cFileName);
				wchar_t* extension        = find_data.cFileName;
				FOR_ELEMS_REV(it, find_data.cFileName, file_name_length)
				{
					if (*it == '.')
					{
						extension = it;
						break;
					}
				}

				if (wcscmp(extension, L".cpp") == 0 || wcscmp(extension, L".h") == 0)
				{
					wchar_t wide_file_path[MAX_PATH];
					if (wcscpy_s(wide_file_path, wide_dir_path) == EINVAL)
					{
						return {};
					}
					if (StringCchCatW(wide_file_path, capacityof(wide_file_path), find_data.cFileName) != S_OK)
					{
						return {};
					}

					char* file_path_data = allocate<char>(arena, static_cast<i64>(wcslen(wide_file_path) + 1));
					u64   file_path_count;
					if (wcstombs_s(&file_path_count, file_path_data, wcslen(wide_file_path) + 1, wide_file_path, _TRUNCATE) || file_path_count != wcslen(wide_file_path) + 1)
					{
						return {};
					}

					StringNode* node = allocate<StringNode>(arena);
					*node =
						{
							.next = file_names,
							.str  =
								{
									.size = static_cast<i64>(wcslen(wide_file_path)),
									.data = file_path_data
								}
						};
					file_names = node;
				}
			}
		}
		else if (GetLastError() == ERROR_NO_MORE_FILES)
		{
			return { true, file_names };
		}
		else
		{
			return {};
		}
	}
}

procedure bool32 read(char** data, i64* size, String file_path, MemoryArena* arena)
{
	HANDLE  handle;
	wchar_t wide_file_path[MAX_PATH];
	u64     wide_file_path_count;
	if (mbstowcs_s(&wide_file_path_count, wide_file_path, file_path.data, static_cast<size_t>(file_path.size)) || static_cast<i64>(wide_file_path_count) != file_path.size + 1)
	{
		return false;
	}

	handle = CreateFileW(wide_file_path, GENERIC_READ, 0, {}, OPEN_EXISTING, 0, {});
	if (handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	DEFER { CloseHandle(handle); };

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(handle, &file_size))
	{
		return false;
	}

	*size = static_cast<i64>(file_size.QuadPart);
	*data = allocate<char>(arena, *size);

	DWORD write_amount;
	if (*size >= (1LL << 32) || !ReadFile(handle, *data, static_cast<DWORD>(*size), &write_amount, 0) || write_amount != *size)
	{
		return false;
	}

	return true;
}

procedure bool32 write(String file_path, String content)
{
	wchar_t wide_file_path[MAX_PATH];
	u64     wide_file_path_count;
	if (mbstowcs_s(&wide_file_path_count, wide_file_path, file_path.data, static_cast<size_t>(file_path.size)) || static_cast<i64>(wide_file_path_count) != file_path.size + 1)
	{
		return false;
	}

	wchar_t dir_path_buffer[MAX_PATH] = {};
	FOR_ELEMS_REV(scan_c, wide_file_path, file_path.size)
	{
		if (*scan_c == L'/' || *scan_c == L'\\')
		{
			FOR_ELEMS(copy_c, wide_file_path, scan_c_index)
			{
				dir_path_buffer[copy_c_index] =
					*copy_c == L'/'
						? L'\\'
						: *copy_c;
			}
			break;
		}
	}

	{
		i32 err = SHCreateDirectoryExW(0, dir_path_buffer, 0);
		if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS)
		{
			return false;
		}
	}

	HANDLE handle = CreateFileW(wide_file_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	DEFER { CloseHandle(handle); };

	DWORD write_amount;
	return content.size < (1LL << 32) && WriteFile(handle, content.data, static_cast<DWORD>(content.size), &write_amount, 0) && write_amount == content.size;
}

struct StringBuilderCharBufferNode
{
	StringBuilderCharBufferNode* next;
	i64  count;
	char buffer[4096];
};

struct StringBuilder
{
	MemoryArena*                 arena;
	i64                          size;
	StringBuilderCharBufferNode  head;
	StringBuilderCharBufferNode* curr;
};

procedure StringBuilder* init_string_builder(MemoryArena* arena)
{
	StringBuilder* builder = allocate<StringBuilder>(arena);
	*builder       = {};
	builder->arena = arena;
	builder->curr  = &builder->head;

	return builder;
}

procedure void append(StringBuilder* builder, String str)
{
	i64 index = 0;
	while (index < str.size)
	{
		if (builder->curr->count == capacityof(builder->curr->buffer))
		{
			if (!builder->curr->next)
			{
				builder->curr->next = allocate<StringBuilderCharBufferNode>(builder->arena);
			}
			builder->curr  = builder->curr->next;
			*builder->curr = {};
		}

		i64 write_amount = min(str.size - index, capacityof(builder->curr->buffer) - builder->curr->count);
		memcpy(builder->curr->buffer + builder->curr->count, str.data + index, static_cast<size_t>(write_amount));
		builder->curr->count += write_amount;
		builder->size        += write_amount;
		index                += write_amount;
	}
}

procedure void append(StringBuilder* builder, StringBuilder* post)
{
	FOR_NODES(&post->head)
	{
		if (it->count)
		{
			append(builder, { it->count, it->buffer });
		}
		else
		{
			break;
		}
	}
}

#define appendf(BUILDER, FORMAT, ...)\
do\
{\
	char APPENDF_BUFFER_[4096];\
	i32 APPENDF_WRITE_AMOUNT_ = sprintf_s(APPENDF_BUFFER_, capacityof(APPENDF_BUFFER_), (FORMAT), __VA_ARGS__);\
	ASSERT(IN_RANGE(APPENDF_WRITE_AMOUNT_, 0, static_cast<i32>(capacityof(APPENDF_BUFFER_))));\
	append((BUILDER), { static_cast<i64>(APPENDF_WRITE_AMOUNT_), APPENDF_BUFFER_ });\
}\
while (false)

procedure String flush(StringBuilder* builder)
{
	char* data = allocate<char>(builder->arena, builder->size);

	i64 write_count = 0;
	FOR_NODES(&builder->head)
	{
		if (it->count)
		{
			memcpy(data + write_count, it->buffer, static_cast<size_t>(it->count));
			write_count += it->count;
			it->count    = 0;
		}
		else
		{
			ASSERT(write_count == builder->size);
			break;
		}
	}

	String result = { builder->size, data };

	builder->size = 0;
	builder->curr = &builder->head;

	return result;
}

enum struct TokenType : u8
{
	null,
	meta,
	preprocessor,
	reserved_symbol,
	identifier,
	number,
	string,
	character
};

enum struct ReservedSymbolType : u8
{
	null,

	// @NOTE@ ASCII characters.

	MULTIBYTE_START  = 127,
	MULTIBYTE_START_ = MULTIBYTE_START - 1,
	a_struct,
	a_union,
	a_enum,
	a_constexpr,
	a_template,
	a_if,
	a_else,
	a_while,
	a_do,
	a_for,
	a_switch,
	a_case,
	a_break,
	a_default,
	a_sizeof,
	plus_2,
	plus_equal,
	sub_2,
	sub_equal,
	asterisk_equal,
	slash_equal,
	pipe_2,
	pipe_equal,
	ampersand_2,
	ampersand_equal,
	less_than_equal,
	greater_than_equal,
	equal_2,
	exclamation_equal,
	less_than_2,
	greater_than_2,
	less_than_2_equal,
	greater_than_2_equal,
	caret_equal,
	percent_equal,
	colon_2,
	sub_greater_than
};

constexpr String RESERVED_STRINGS[] =
	{
		String("struct"),
		String("union"),
		String("enum"),
		String("constexpr"),
		String("template"),
		String("if"),
		String("else"),
		String("while"),
		String("do"),
		String("for"),
		String("switch"),
		String("case"),
		String("break"),
		String("default"),
		String("sizeof"),
		String("++"),
		String("+="),
		String("--"),
		String("-="),
		String("*="),
		String("/="),
		String("||"),
		String("|="),
		String("&&"),
		String("&="),
		String("<="),
		String(">="),
		String("=="),
		String("!="),
		String("<<"),
		String(">>"),
		String("<<="),
		String(">>="),
		String("^="),
		String("%="),
		String("::"),
		String("->")
	};

struct Token
{
	TokenType type;
	String    text;
	i32       line_index;

	union
	{
		struct
		{
			String info;
		} meta;

		struct
		{
			ReservedSymbolType type;
		} reserved_symbol;
	};
};

struct TokenBufferNode
{
	TokenBufferNode* next;
	TokenBufferNode* prev;
	i64              count;
	Token            buffer[4096];
};

struct Tokenizer
{
	String           file_path;
	i64              file_size;
	char*            file_data;
	i64              curr_index_in_node;
	TokenBufferNode* curr_node;
};

procedure Tokenizer init_tokenizer(String file_path, MemoryArena* arena)
{
	Tokenizer tokenizer =
		{
			.file_path = file_path
		};

	if (!read(&tokenizer.file_data, &tokenizer.file_size, file_path, arena))
	{
		return {};
	}

	tokenizer.curr_node = allocate<TokenBufferNode>(arena);
	*tokenizer.curr_node = {};

	TokenBufferNode* head = tokenizer.curr_node;

	char* curr_read         = tokenizer.file_data;
	i32   curr_line_index   = 0;
	while (curr_read < tokenizer.file_data + tokenizer.file_size)
	{
		Token token =
			{
				.text       = { 0, curr_read },
				.line_index = curr_line_index,
			};

		lambda peek_read =
			[&](i32 offset)
			{
				if (IN_RANGE(curr_read + offset, tokenizer.file_data, tokenizer.file_data + tokenizer.file_size))
				{
					return curr_read[offset];
				}
				else
				{
					return '\0';
				}
			};

		lambda inc =
			[&](i64 offset)
			{
				offset = min(offset, static_cast<i64>(tokenizer.file_data + tokenizer.file_size - curr_read));
				FOR_RANGE(offset)
				{
					if (curr_read[0] == '\n')
					{
						curr_line_index += 1;
					}
					curr_read       += 1;
					token.text.size += 1;
				}
			};

		if (peek_read(0) == ' ' || is_whitespace(peek_read(0)))
		{
			inc(1);
		}
		else if (peek_read(0) == '/' && peek_read(1) == '*')
		{
			while (peek_read(0) && !(peek_read(-2) == '*' && peek_read(-1) == '/'))
			{
				inc(1);
			}
		}
		else if (peek_read(0) == '/' && peek_read(1) == '/')
		{
			inc(2);

			while (peek_read(0) && (peek_read(0) == ' ' || peek_read(0) == '\t'))
			{
				inc(1);
			}

			String comment = { 0, curr_read };

			while (peek_read(0) && peek_read(0) != '\r' && peek_read(0) != '\n')
			{
				comment.size += 1;
				inc(1);
			}

			if (starts_with(comment, String("@META@")))
			{
				token.type      = TokenType::meta;
				token.text      = rtrim_whitespace(token.text);
				token.meta.info = trim_whitespace(ltrim(comment, String("@META@").size));
			}
		}
		else if (peek_read(0) == '#')
		{
			inc(1);
			token.type = TokenType::preprocessor;

			bool32 escaped = false;
			while (peek_read(0) && IMPLIES(!escaped, peek_read(0) != '\n') && IMPLIES(peek_read(0) == '/', peek_read(1) != '/' && peek_read(1) != '*'))
			{
				if (peek_read(0) == '\\')
				{
					escaped = !escaped;
				}
				else if (peek_read(0) != '\r')
				{
					escaped = false;
				}
				inc(1);
			}
			token.text = rtrim_whitespace(token.text);
		}
		else if (is_alpha(peek_read(0)) || peek_read(0) == '_')
		{
			token.type = TokenType::identifier;

			inc(1);
			while (is_alpha(peek_read(0)) || is_digit(peek_read(0)) || peek_read(0) == '_')
			{
				inc(1);
			}

			FOR_ELEMS(RESERVED_STRINGS)
			{
				if (*it == token.text)
				{
					token.type                 = TokenType::reserved_symbol;
					token.reserved_symbol.type = static_cast<ReservedSymbolType>(static_cast<i32>(ReservedSymbolType::MULTIBYTE_START) + it_index);
					break;
				}
			}
		}
		else if (is_digit(peek_read(0)) || (peek_read(0) == '.' && is_digit(peek_read(1))))
		{
			// @TODO@ Proper handling of suffixes, prefixes, and decimals.
			token.type = TokenType::number;

			inc(1);
			while (is_alpha(peek_read(0)) || is_digit(peek_read(0)) || peek_read(0) == '.' || peek_read(0) == '\'')
			{
				inc(1);
			}
		}
		else if (peek_read(0) == '"')
		{
			inc(1);
			token.type = TokenType::string;

			bool32 escaped = false;
			while (peek_read(0) && IMPLIES(!escaped, peek_read(0) != '"' && peek_read(0) != '\r' && peek_read(0) != '\n'))
			{
				if (peek_read(0) == '\\')
				{
					escaped = !escaped;
				}
				else if (peek_read(0) != '\r')
				{
					escaped = false;
				}
				inc(1);
			}

			// @TODO@ Report that there is an unclosed string literal.

			if (peek_read(0) == '"')
			{
				inc(0);
			}
		}
		else if (peek_read(0) == '\'')
		{
			token.type = TokenType::character;

			inc(1);
			if (peek_read(0) == '\\')
			{
				inc(1);
			}

			if (peek_read(0) == '\'')
			{
				inc(1);
			}
		}
		else
		{
			token.type = TokenType::reserved_symbol;

			FOR_ELEMS(RESERVED_STRINGS)
			{
				if (curr_read + it->size <= tokenizer.file_data + tokenizer.file_size && String { it->size, curr_read } == *it)
				{
					token.reserved_symbol.type = static_cast<ReservedSymbolType>(static_cast<i32>(ReservedSymbolType::MULTIBYTE_START) + it_index);
					inc(it->size);
					goto BREAK;
				}
			}
			{
				token.reserved_symbol.type = static_cast<ReservedSymbolType>(peek_read(0));
				inc(1);
			}
			BREAK:;
		}

		if (token.type != TokenType::null)
		{
			if (tokenizer.curr_node->count == capacityof(tokenizer.curr_node->buffer))
			{
				tokenizer.curr_node->next       = allocate<TokenBufferNode>(arena);
				*tokenizer.curr_node->next      = {};
				tokenizer.curr_node->next->prev = tokenizer.curr_node;
				tokenizer.curr_node             = tokenizer.curr_node->next;
			}

			tokenizer.curr_node->buffer[tokenizer.curr_node->count] = token;
			tokenizer.curr_node->count += 1;
		}
	}

	tokenizer.curr_node = head;

	return tokenizer;
}

procedure bool32 is_reserved_symbol(Token token, ReservedSymbolType type)
{
	return token.type == TokenType::reserved_symbol && token.reserved_symbol.type == type;
}

procedure bool32 is_reserved_symbol(Token token, char type)
{
	return token.type == TokenType::reserved_symbol && token.reserved_symbol.type == static_cast<ReservedSymbolType>(type);
}

procedure Token shift(Tokenizer* tokenizer, i32 offset)
{
	i32 remaining = offset;
	i32 step      = sign(offset);

	while (remaining)
	{
		tokenizer->curr_index_in_node += step;
		if (tokenizer->curr_index_in_node < 0)
		{
			if (tokenizer->curr_node->prev)
			{
				ASSERT(tokenizer->curr_node->prev->count == capacityof(tokenizer->curr_node->prev->buffer));
				tokenizer->curr_node          = tokenizer->curr_node->prev;
				tokenizer->curr_index_in_node = capacityof(tokenizer->curr_node->buffer) - 1;
			}
			else
			{
				tokenizer->curr_index_in_node = 0;
				return {};
			}
		}
		else if (static_cast<i64>(tokenizer->curr_index_in_node) >= tokenizer->curr_node->count)
		{
			if (tokenizer->curr_node->next)
			{
				ASSERT(tokenizer->curr_node->count == capacityof(tokenizer->curr_node->buffer));
				tokenizer->curr_node          = tokenizer->curr_node->next;
				tokenizer->curr_index_in_node = 0;
			}
			else
			{
				ASSERT(tokenizer->curr_node->count);
				tokenizer->curr_index_in_node = static_cast<i32>(tokenizer->curr_node->count) - 1;
				return {};
			}
		}

		remaining -= step;
	}

	return tokenizer->curr_node->buffer[tokenizer->curr_index_in_node];
}

procedure void report(String message, Tokenizer* tokenizer)
{
	Token reported_token = shift(tokenizer, 0);

	String line;
	{
		Tokenizer scan_tokenizer = *tokenizer;
		while (true)
		{
			Token curr_token = shift(&scan_tokenizer,  0);
			Token prev_token = shift(&scan_tokenizer, -1);
			if (prev_token.type == TokenType::null || prev_token.line_index != curr_token.line_index)
			{
				line.data = curr_token.text.data;
				break;
			}
		}
	}
	{
		Tokenizer scan_tokenizer = *tokenizer;
		while (true)
		{
			Token curr_token = shift(&scan_tokenizer, 0);
			Token next_token = shift(&scan_tokenizer, 1);
			if (next_token.type == TokenType::null || next_token.line_index != curr_token.line_index)
			{
				line.size = static_cast<i64>(curr_token.text.data + curr_token.text.size - line.data);
				break;
			}
		}
	}

	printf(":: (%d) %.*s : %.*s\n", reported_token.line_index + 1, PASS_ISTR(tokenizer->file_path), PASS_ISTR(message));
	printf(":: `%.*s`\n", PASS_ISTR(line));

	ASSERT(reported_token.text.data >= line.data);
	printf("::  %*s", static_cast<i32>(reported_token.text.data - line.data), "");
	FOR_RANGE(reported_token.text.size)
	{
		printf("~");
	}
	printf("\n");
}

struct TokenNode
{
	TokenNode* next;
	Token      token;
};

enum struct MetaTypeType
{
	null,
	atom,
	array,
	container,
	enumerator
};

struct MetaTypeRef
{
	MetaTypeType ref_type;
	union
	{
		struct MetaTypeAtom*       atom;
		struct MetaTypeArray*      array;
		struct MetaTypeContainer*  container;
		struct MetaTypeEnumerator* enumerator;
	};
};

struct MetaTypeAtom
{
	String meta;
	String name;
};

struct MetaTypeArray
{
	String             meta;
	struct MetaTypeRef underlying_type;
	TokenNode*         capacity;
};

struct MetaTypeContainer
{
	String                      meta;
	bool32                      is_union;
	String                      name;
	struct MetaDeclarationNode* declarations;
};

struct MetaTypeEnumeratorMemberNode
{
	MetaTypeEnumeratorMemberNode* next;
	String                        meta;
	String                        name;
	TokenNode*                    definition;
};

struct MetaTypeEnumerator
{
	String                        meta;
	String                        name;
	MetaTypeAtom                  underlying_type;
	MetaTypeEnumeratorMemberNode* members;
};

struct MetaType
{
	MetaTypeType type;
	union
	{
		MetaTypeAtom       atom;
		MetaTypeArray      array;
		MetaTypeContainer  container;
		MetaTypeEnumerator enumerator;
	};
};

struct MetaDeclaration
{
	String      meta;
	MetaTypeRef underlying_type;
	String      name;
	TokenNode*  assignment;
};

struct MetaDeclarationNode
{
	MetaDeclarationNode* next;
	MetaDeclaration      declaration;
};

procedure bool32 parse_enumerator(MetaTypeEnumerator* enumerator, Tokenizer* tokenizer, MemoryArena* arena)
{
	Token token = shift(tokenizer, 0);

	if (!is_reserved_symbol(token, ReservedSymbolType::a_enum))
	{
		report(String("Expected `enum` here."), tokenizer);
		return false;
	}

	token = shift(tokenizer, 1);
	if (is_reserved_symbol(token, ReservedSymbolType::a_struct))
	{
		token = shift(tokenizer, 1);
	}
	if (token.type != TokenType::identifier)
	{
		report(String("Expected the enumerator's name."), tokenizer);
		return false;
	}
	enumerator->name = token.text;

	token = shift(tokenizer, 1);
	if (!is_reserved_symbol(token, ':'))
	{
		report(String("Expected a colon to denote underlying type."), tokenizer);
		return false;
	}

	token = shift(tokenizer, 1);
	if (token.type != TokenType::identifier)
	{
		report(String("Expected an underlying type."), tokenizer);
		return false;
	}
	enumerator->underlying_type.name = token.text;

	token = shift(tokenizer, 1);
	if (token.type == TokenType::meta)
	{
		enumerator->meta = token.meta.info;
		token            = shift(tokenizer, 1);
	}
	if (!is_reserved_symbol(token, '{'))
	{
		report(String("Expected an opening curly brace."), tokenizer);
		return false;
	}
	token = shift(tokenizer, 1);

	MetaTypeEnumeratorMemberNode** nil = &enumerator->members;
	while (true)
	{
		token = shift(tokenizer, 0);
		if (is_reserved_symbol(token, '}'))
		{
			token = shift(tokenizer, 1);
			return true;
		}
		else if (token.type == TokenType::identifier)
		{
			*nil  = allocate<MetaTypeEnumeratorMemberNode>(arena);
			**nil = { .name = token.text };

			token = shift(tokenizer, 1);
			if (is_reserved_symbol(token, '='))
			{
				TokenNode** token_nil = &(*nil)->definition;
				while (true)
				{
					token = shift(tokenizer, 1);
					if (token.type == TokenType::null)
					{
						printf(":: EOF found when parsing enumerator for `%.*s`.\n", PASS_ISTR(enumerator->name));
						return false;
					}
					else if (is_reserved_symbol(token, ','))
					{
						token = shift(tokenizer, 1);
						break;
					}
					else if (is_reserved_symbol(token, '}'))
					{
						break;
					}
					else
					{
						*token_nil  = allocate<TokenNode>(arena);
						**token_nil = { .token = token };
						token_nil   = &(*token_nil)->next;
					}
				}
			}
			if (is_reserved_symbol(token, ','))
			{
				token = shift(tokenizer, 1);
			}
			if (token.type == TokenType::meta)
			{
				(*nil)->meta = token.meta.info;
				token = shift(tokenizer, 1);
			}

			nil = &(*nil)->next;
		}
		else
		{
			report(String("Unexpected token."), tokenizer);
			return false;
		}
	}
}

procedure bool32 parse_declaration(MetaDeclaration* declaration, Tokenizer* tokenizer, MemoryArena* arena);
procedure bool32 parse_container(MetaTypeContainer* container, Tokenizer* tokenizer, MemoryArena* arena)
{
	*container = {};

	Token token = shift(tokenizer, 0);

	if (is_reserved_symbol(token, ReservedSymbolType::a_struct))
	{
	}
	else if (is_reserved_symbol(token, ReservedSymbolType::a_union))
	{
		container->is_union = true;
	}
	else
	{
		report(String("Expected `struct` or `union` here."), tokenizer);
		return false;
	}

	token = shift(tokenizer, 1);
	if (token.type == TokenType::identifier)
	{
		container->name = token.text;
		token = shift(tokenizer, 1);
	}
	if (token.type == TokenType::meta)
	{
		container->meta = token.meta.info;
		token = shift(tokenizer, 1);
	}
	if (!is_reserved_symbol(token, '{'))
	{
		report(String("Expected an opening curly brace."), tokenizer);
		return false;
	}
	token = shift(tokenizer, 1);

	MetaDeclarationNode** nil = &container->declarations;
	while (true)
	{
		token = shift(tokenizer, 0);
		if (token.type == TokenType::null)
		{
			if (+container->name)
			{
				printf(":: EOF found when parsing container for `%.*s`.\n", PASS_ISTR(container->name));
			}
			else
			{
				printf(":: EOF found when parsing nameless container.\n");
			}
			return false;
		}
		else if (is_reserved_symbol(token, '}'))
		{
			shift(tokenizer, 1);
			return true;
		}
		else
		{
			*nil  = allocate<MetaDeclarationNode>(arena);
			**nil = {};
			if (!parse_declaration(&(*nil)->declaration, tokenizer, arena))
			{
				if (+container->name)
				{
					printf(":: Failed to parse declaration in container for `%.*s`.\n", PASS_ISTR(container->name));
				}
				else
				{
					printf(":: Failed to parse declaration in container.\n");
				}

				return false;
			}

			token = shift(tokenizer, 0);
			if (!is_reserved_symbol(token, ';'))
			{
				report(String("Expected a semicolon."), tokenizer);
				return false;
			}

			token = shift(tokenizer, 1);
			if (token.type == TokenType::meta)
			{
				(*nil)->declaration.meta = token.meta.info;
				token = shift(tokenizer, 1);
			}

			nil = &(*nil)->next;
		}
	}
}

procedure bool32 parse_declaration(MetaDeclaration* declaration, Tokenizer* tokenizer, MemoryArena* arena)
{
	*declaration = {};

	Token token = shift(tokenizer, 0);
	if (is_reserved_symbol(token, ReservedSymbolType::a_union) || is_reserved_symbol(token, ReservedSymbolType::a_struct))
	{
		declaration->underlying_type = { MetaTypeType::container, { .container = allocate<MetaTypeContainer>(arena) } };
		if (!parse_container(declaration->underlying_type.container, tokenizer, arena))
		{
			report(String("Failed to parse declaration."), tokenizer);
			return false;
		}
		token = shift(tokenizer, 0);
	}
	else
	{
		if (token.type != TokenType::identifier)
		{
			report(String("Expected type."), tokenizer);
			return false;
		}
		declaration->underlying_type = { MetaTypeType::atom, { .atom = allocate<MetaTypeAtom>(arena) } };
		*declaration->underlying_type.atom = { .name = token.text };
		token = shift(tokenizer, 1);
	}

	if (token.type != TokenType::identifier)
	{
		report(String("Expected name."), tokenizer);
		return false;
	}
	declaration->name = token.text;

	token = shift(tokenizer, 1);
	if (is_reserved_symbol(token, '['))
	{
		{
			MetaTypeRef array_type = { MetaTypeType::array, { .array = allocate<MetaTypeArray>(arena) } };
			*array_type.array            = { .underlying_type = declaration->underlying_type };
			declaration->underlying_type = array_type;
		}
		TokenNode** nil = &declaration->underlying_type.array->capacity;

		while (true)
		{
			token = shift(tokenizer, 1);
			if (token.type == TokenType::null)
			{
				printf(":: EOF found when attempting to store tokens of array capacity of `%.*s`.\n", PASS_ISTR(declaration->name));
				return false;
			}
			else if (is_reserved_symbol(token, ']'))
			{
				token = shift(tokenizer, 1);
				break;
			}
			else
			{
				*nil  = allocate<TokenNode>(arena);
				**nil = { .token = token };
				nil   = &(*nil)->next;
			}
		}
	}

	return true;
}

procedure bool32 is_valid_identifier(String str)
{
	FOR_STR(str)
	{
		if (*it != '_' && !is_alpha(*it) && IMPLIES(is_digit(*it), it_index == 0))
		{
			return false;
		}
	}

	return true;
}

procedure bool32 parse_include_meta(String* file_path, String* file_name, String* file_extension, String* meta_operation, String include_directive)
{
	*file_path = include_directive;
	if (!file_path->size || file_path->data[0] != '#')
	{
		return false;
	}

	*file_path = ltrim_whitespace(ltrim(*file_path, 1));

	if (!starts_with(*file_path, String("include")))
	{
		return false;
	}

	*file_path = trim(ltrim_whitespace(ltrim(*file_path, String("include").size)), 1, 1);

	if (!starts_with(*file_path, String("META/")))
	{
		return false;
	}

	*meta_operation     = ltrim(*file_path, String("META/").size);
	*file_name          = *meta_operation;
	FOR_STR(*meta_operation)
	{
		if (*it == '/')
		{
			meta_operation->size = it_index;
			*file_name           = ltrim(*file_name, it_index + 1);
			break;
		}
	}
	FOR_STR(*file_name)
	{
		if (*it == '.')
		{
			*file_extension = ltrim(*file_name, it_index);
			file_name->size = it_index;
			break;
		}
	}

	return true;
}

int main()
{
	//DEFER { DEBUG_STDOUT_HALT(); };

	LARGE_INTEGER performance_counter_start;
	QueryPerformanceCounter(&performance_counter_start);
	DEFER
	{
		LARGE_INTEGER performance_counter_end;
		QueryPerformanceCounter(&performance_counter_end);

		LARGE_INTEGER performance_frequency;
		QueryPerformanceFrequency(&performance_frequency);

		printf(":: metaprogram.exe : %.0fms\n", 1000.0 * static_cast<f64>(performance_counter_end.QuadPart - performance_counter_start.QuadPart) / static_cast<f64>(performance_frequency.QuadPart));
	};

	MemoryArena main_arena = {};
	main_arena.size = MEBIBYTES_OF(1);
	main_arena.data = reinterpret_cast<byte*>(malloc(static_cast<size_t>(main_arena.size)));
	if (!main_arena.data)
	{
		return 1;
	}
	DEFER { free(main_arena.data); };

	FilePathsInResult subject_file_paths = file_paths_in(String(SRC_DIR), &main_arena);
	if (!subject_file_paths.success)
	{
		printf(":: Failed to get list of file paths in `" SRC_DIR "`\n");
		return 1;
	}

	FOR_NODES(subject_file_path, subject_file_paths.value)
	{
		DEFER_ARENA_RESET(&main_arena);

		Tokenizer main_tokenizer = init_tokenizer(subject_file_path->str, &main_arena);

		for (Token main_token = shift(&main_tokenizer, 0); main_token.type != TokenType::null; main_token = shift(&main_tokenizer, 1))
		{
			String file_path;
			String file_name;
			String file_extension;
			String meta_operation;
			if (!parse_include_meta(&file_path, &file_name, &file_extension, &meta_operation, main_token.text))
			{
				continue;
			}

			DEFER_ARENA_RESET(&main_arena);

			if (meta_operation == String("enum"))
			{
				Tokenizer meta_tokenizer = main_tokenizer;
				Token     meta_token     = shift(&meta_tokenizer, 0);

				while (true)
				{
					meta_token = shift(&meta_tokenizer, -1);

					if (is_reserved_symbol(meta_token, ReservedSymbolType::a_enum))
					{
						meta_token = shift(&meta_tokenizer, 1);
						if (meta_token.type == TokenType::identifier && meta_token.text == String("Cardinal"))
						{
							meta_token = shift(&meta_tokenizer, -1);
							break;
						}
						else
						{
							meta_token = shift(&meta_tokenizer, -1);
						}
					}

					if (meta_token.type == TokenType::null)
					{
						report(String("Failed to find enum declaration."), &main_tokenizer);
						return 1;
					}
				}

				MetaTypeEnumerator enumerator;
				if (!parse_enumerator(&enumerator, &meta_tokenizer, &main_arena))
				{
					report(String("Failed to parse enumerator for meta operation."), &main_tokenizer);
					return 1;
				}

				StringBuilder* meta_builder = init_string_builder(&main_arena);

				appendf
				(
					meta_builder,
					"global constexpr struct META_%.*s_t { %.*s enumerator; %.*s value; union { String str; struct { byte PADDING_[offsetof(String, data)]; strlit cstr; }; }; ",
					PASS_ISTR(enumerator.name), PASS_ISTR(enumerator.name), PASS_ISTR(enumerator.underlying_type.name)
				);

				if (+enumerator.meta)
				{
					appendf(meta_builder, "%.*s ", PASS_ISTR(enumerator.meta));
				}

				appendf(meta_builder, "} META_%.*s[] =\n\t{\n", PASS_ISTR(enumerator.name));

				FOR_NODES(enumerator.members)
				{
					appendf
					(
						meta_builder,
						"\t\t{ %.*s::%.*s, static_cast<%.*s>(%.*s::%.*s), String(\"%.*s\") ",
						PASS_ISTR(enumerator.name),
						PASS_ISTR(it->name),
						PASS_ISTR(enumerator.underlying_type.name),
						PASS_ISTR(enumerator.name),
						PASS_ISTR(it->name),
						PASS_ISTR(it->name)
					);

					if (it->meta.data)
					{
						if (+enumerator.meta)
						{
							appendf(meta_builder, ", %.*s", PASS_ISTR(it->meta));
						}
						else
						{
							report(String("A member has a meta tag when the enumerator declaration does not."), &main_tokenizer);
							return 1;
						}
					}
					else
					{
						report(String("A member is missing a meta tag."), &main_tokenizer);
						return 1;
					}

					appendf(meta_builder, " }");
					if (it->next)
					{
						appendf(meta_builder, ",");
					}
					appendf(meta_builder, "\n");
				}

				appendf(meta_builder, "\t};\n");
				String meta_data = flush(meta_builder);

				appendf(meta_builder, SRC_DIR);
				append(meta_builder, file_path);

				if (!write(flush(meta_builder), meta_data))
				{
					report(String("Failed to write meta data to file."), &main_tokenizer);
					return 1;
				}
			}
			else if (meta_operation == String("asset"))
			{
				Tokenizer meta_tokenizer = main_tokenizer;
				Token     meta_token     = shift(&meta_tokenizer, 0);

				meta_token = shift(&meta_tokenizer, -1);
				if (!is_reserved_symbol(meta_token, ';'))
				{
					report(String("Expected semicolon to denote the end of a union declaration."), &meta_tokenizer);
					return 1;
				}

				meta_token = shift(&meta_tokenizer, -1);
				if (!is_reserved_symbol(meta_token, '}'))
				{
					report(String("Expected a closing curly brace to denote the end of a union declaration."), &meta_tokenizer);
					return 1;
				}

				for (i32 depth= 1; depth;)
				{
					meta_token = shift(&meta_tokenizer, -1);

					if (meta_token.type == TokenType::null)
					{
						report(String("Failed to find the corresponding union for the meta operation."), &main_tokenizer);
						return 1;
					}
					else if (is_reserved_symbol(meta_token, '{'))
					{
						depth -= 1;
						if (depth == 0)
						{
							meta_token = shift(&meta_tokenizer, -1);
							if (!is_reserved_symbol(meta_token, ReservedSymbolType::a_union))
							{
								report(String("Expected keyword `union` here."), &meta_tokenizer);
								report(String("Failed to do meta operation."), &main_tokenizer);
								return 1;
							}
						}
					}
					else if (is_reserved_symbol(meta_token, '}'))
					{
						depth += 1;
					}
				}

				MetaTypeContainer container;
				if (!parse_container(&container, &meta_tokenizer, &main_arena))
				{
					report(String("Failed to parse container for meta operation."), &main_tokenizer);
					return 1;
				}

				if
				(
					!container.is_union
					|| !container.declarations       || container.declarations->      declaration.underlying_type.ref_type != MetaTypeType::container || container.declarations->declaration.underlying_type.container->is_union
					|| !container.declarations->next || container.declarations->next->declaration.underlying_type.ref_type != MetaTypeType::array
				)
				{
					report(String("Invalid layout for meta operation."), &main_tokenizer);
					return 1;
				}

				StringBuilder* meta_builder  = init_string_builder(&main_arena);
				StringBuilder* defer_builder = init_string_builder(&main_arena);

				appendf(meta_builder, "global constexpr String META_%.*s[] =\n\t{\n", PASS_ISTR(file_name));

				FOR_NODES(container.declarations->declaration.underlying_type.container->declarations)
				{
					if (!it->declaration.meta.size)
					{
						report(String("Not all assets have a file path."), &main_tokenizer);
						return 1;
					}

					appendf(meta_builder, "\t\t");

					i32 asset_path_count = 0;
					for (String remaining = ltrim_whitespace(it->declaration.meta); remaining.size; remaining = ltrim_whitespace(remaining))
					{
						asset_path_count += 1;

						String asset_path = remaining;
						FOR_STR(c, asset_path)
						{
							if (*c == ',')
							{
								asset_path.size = c_index;
								break;
							}
						}

						remaining  = ltrim_whitespace(ltrim(remaining, asset_path.size + 1));
						asset_path = rtrim_whitespace(asset_path);

						appendf(meta_builder, "String(DATA_DIR \"%.*s\")", PASS_ISTR(asset_path));

						if (remaining.size)
						{
							appendf(meta_builder, ", ");
						}
						else if (it->next)
						{
							appendf(meta_builder, ",");
						}
					}
					appendf(meta_builder, "\n");

					appendf
					(
						defer_builder,
						"static_assert(sizeof(%.*s.%.*s) / sizeof(%.*s) == %d, \"(%.*s.)(%d) :: The amount of assets and file paths are not the same.\");\n",
						PASS_ISTR(container.declarations->declaration.name),
						PASS_ISTR(it->declaration.name),
						PASS_ISTR(container.declarations->next->declaration.underlying_type.array->underlying_type.atom->name),
						asset_path_count,
						PASS_ISTR(container.declarations->declaration.name),
						PASS_ISTR(it->declaration.name),
						main_token.line_index + 1
					);
				}

				appendf(meta_builder, "\t};\n");

				append(meta_builder, defer_builder);

				String meta_data = flush(meta_builder);
				append(meta_builder, String(SRC_DIR));
				append(meta_builder, file_path);

				if (!write(flush(meta_builder), meta_data))
				{
					report(String("Failed to write meta data to file."), &main_tokenizer);
					return 1;
				}
			}
			else if (meta_operation == String("kind"))
			{
				bool32 is_forwarded = starts_with(file_extension, String(".forward"));
				String forwarded_file_path;
				String forwarded_file_name;
				String forwarded_file_extension;
				String forwarded_meta_operation;
				if (is_forwarded)
				{
					forwarded_file_path      = file_path;
					forwarded_file_name      = file_name;
					forwarded_file_extension = file_extension;
					forwarded_meta_operation = meta_operation;

					while (true)
					{
						main_token = shift(&main_tokenizer, 1);
						if (main_token.type == TokenType::null)
						{
							printf(":: EOF found when trying to find the corresponding meta operation for `%.*s`.\n", PASS_ISTR(file_path));
							return 1;
						}
						else if
						(
							parse_include_meta(&file_path, &file_name, &file_extension, &meta_operation, main_token.text)
							&& file_name == forwarded_file_name
							&& meta_operation == forwarded_meta_operation
						)
						{
							break;
						}
					}
				}

				Tokenizer    meta_tokenizer = main_tokenizer;
				StringNode*  members        = {};
				Token        meta_token     = shift(&meta_tokenizer, 1);
				if (meta_token.type != TokenType::meta)
				{
					report(String("Expected meta tag to list the members."), &meta_tokenizer);
					return 1;
				}

				{
					String       remaining = meta_token.meta.info;
					StringNode** nil       = &members;
					while (remaining.size)
					{
						String member = remaining;
						FOR_STR(remaining)
						{
							if (*it == ',')
							{
								member.size = it_index;
								break;
							}
						}
						remaining = ltrim(remaining, member.size + 1);
						member    = rtrim_whitespace(member);

						if (member.size)
						{
							*nil  = allocate<StringNode>(&main_arena);
							**nil = { .str = member };
							nil   = &(*nil)->next;
						}

						remaining = ltrim_whitespace(remaining);
					}
				}

				if (!members)
				{
					report(String("No members listed."), &main_tokenizer);
					return 1;
				}

				StringBuilder* meta_builder = init_string_builder(&main_arena);

				{
					appendf(meta_builder, "enum struct %.*sType : u8\n{\n\tnull", PASS_ISTR(file_name));
					FOR_NODES(members)
					{
						appendf(meta_builder, ",\n\t%.*s", PASS_ISTR(it->str));
					}
					appendf(meta_builder, "\n};\n");

					appendf(meta_builder, "\n");

					appendf(meta_builder, "struct %.*sRef\n{\n\t%.*sType ref_type;\n\tunion\n\t{\n", PASS_ISTR(file_name), PASS_ISTR(file_name));
					FOR_NODES(members)
					{
						appendf(meta_builder, "\t\tstruct %.*s* %.*s_;\n", PASS_ISTR(it->str), PASS_ISTR(it->str));
					}
					append(meta_builder, String("\t};\n"));
					appendf(meta_builder, "};\n");
				}

				if (is_forwarded)
				{
					String forwarded_meta_data = flush(meta_builder);
					append(meta_builder, String(SRC_DIR));
					append(meta_builder, forwarded_file_path);

					if (!write(flush(meta_builder), forwarded_meta_data))
					{
						report(String("Failed to write forwarded meta data to file."), &main_tokenizer);
						return 1;
					}
				}
				else
				{
					appendf(meta_builder, "\n");
				}

				{
					appendf(meta_builder, "struct %.*s\n{\n\t%.*sType type;\n\tunion\n\t{\n", PASS_ISTR(file_name), PASS_ISTR(file_name));
					FOR_NODES(members)
					{
						appendf(meta_builder, "\t\t%.*s %.*s_;\n", PASS_ISTR(it->str), PASS_ISTR(it->str));
					}
					appendf(meta_builder, "\t};\n};\n");
				}

				appendf(meta_builder, "\n");

				{
					appendf(meta_builder, "#pragma clang diagnostic push\n#pragma clang diagnostic ignored \"-Wunused-function\"\n");
					appendf(meta_builder, "procedure constexpr %.*sRef ref(%.*s* x) { return { x->type, { .%.*s_ = &x->%.*s_ } }; }\n", PASS_ISTR(file_name), PASS_ISTR(file_name), PASS_ISTR(members->str), PASS_ISTR(members->str));
					FOR_NODES(members)
					{
						// @TODO@ If a member is used in two different kinds, there will be ambiguity as to what kind to widen to or to make a ref as.
						appendf(meta_builder, "procedure constexpr %.*s widen(const %.*s& x) { return { %.*sType::%.*s, { .%.*s_ = x } }; }\n", PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(it->str));
						appendf(meta_builder, "procedure constexpr %.*sRef ref(%.*s* x) { return { %.*sType::%.*s, { .%.*s_ = x } }; }\n", PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(it->str));
						appendf(meta_builder, "procedure bool32 deref(%.*s** x, %.*s*   y) { *x = &y->%.*s_; return y->type     == %.*sType::%.*s; }\n", PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str));
						appendf(meta_builder, "procedure bool32 deref(%.*s** x, %.*sRef y) { *x =  y. %.*s_; return y. ref_type == %.*sType::%.*s; }\n", PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str), PASS_ISTR(file_name), PASS_ISTR(it->str));
					}

					append(meta_builder, String("#pragma clang diagnostic pop\n"));
				}

				String meta_data = flush(meta_builder);
				append(meta_builder, String(SRC_DIR));
				append(meta_builder, file_path);

				if (!write(flush(meta_builder), meta_data))
				{
					report(String("Failed to write meta data to file."), &main_tokenizer);
					return 1;
				}
			}
			else if (meta_operation == String("flag"))
			{
				Tokenizer meta_tokenizer = main_tokenizer;
				Token     meta_token = shift(&meta_tokenizer, -1);

				while (true)
				{
					if (meta_token.type == TokenType::null)
					{
						report(String("Failed to find the corresponding enumerator for the meta operation."), &main_tokenizer);
						return 1;
					}
					else if (meta_token.type == TokenType::identifier && meta_token.text == file_name)
					{
						meta_token = shift(&meta_tokenizer, -1);
						if (is_reserved_symbol(meta_token, ReservedSymbolType::a_struct))
						{
							meta_token = shift(&meta_tokenizer, -1);
						}
						if (is_reserved_symbol(meta_token, ReservedSymbolType::a_enum))
						{
							break;
						}
					}

					meta_token = shift(&meta_tokenizer, -1);
				}

				MetaTypeEnumerator enumerator;
				if (!parse_enumerator(&enumerator, &meta_tokenizer, &main_arena))
				{
					report(String("Failed to parse enumerator for meta operation."), &main_tokenizer);
					return 1;
				}

				StringBuilder* meta_builder = init_string_builder(&main_arena);

				appendf(meta_builder, "#pragma clang diagnostic push\n#pragma clang diagnostic ignored \"-Wunused-function\"\n");

				#define TYPE PASS_ISTR(enumerator.underlying_type.name)
				#define NAME PASS_ISTR(file_name)
				appendf(meta_builder, "procedure constexpr %.*s operator+ (const %.*s& a) { return static_cast<%.*s>(a); }\n", TYPE, NAME, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator~ (const %.*s& a) { return static_cast<%.*s>(~static_cast<%.*s>(a)); }\n", NAME, NAME, NAME, TYPE);

				appendf(meta_builder, "procedure constexpr %.*s operator&  (const %.*s& a, const %.*s& b) { return     static_cast<%.*s>(static_cast<%.*s>(a) & static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator|  (const %.*s& a, const %.*s& b) { return     static_cast<%.*s>(static_cast<%.*s>(a) | static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator^  (const %.*s& a, const %.*s& b) { return     static_cast<%.*s>(static_cast<%.*s>(a) ^ static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator&= (      %.*s& a, const %.*s& b) { return a = static_cast<%.*s>(static_cast<%.*s>(a) & static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator|= (      %.*s& a, const %.*s& b) { return a = static_cast<%.*s>(static_cast<%.*s>(a) | static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);
				appendf(meta_builder, "procedure constexpr %.*s operator^= (      %.*s& a, const %.*s& b) { return a = static_cast<%.*s>(static_cast<%.*s>(a) ^ static_cast<%.*s>(b)); }\n", NAME, NAME, NAME, NAME, TYPE, TYPE);

				appendf(meta_builder, "procedure constexpr %.*s operator<< (const %.*s& a, const %.*s n) { return     static_cast<%.*s>(static_cast<%.*s>(a) << n); }\n", NAME, NAME, TYPE, NAME);
				appendf(meta_builder, "procedure constexpr %.*s operator>> (const %.*s& a, const %.*s n) { return     static_cast<%.*s>(static_cast<%.*s>(a) >> n); }\n", NAME, NAME, TYPE, NAME);
				appendf(meta_builder, "procedure constexpr %.*s operator<<=(      %.*s& a, const %.*s n) { return a = static_cast<%.*s>(static_cast<%.*s>(a) << n); }\n", NAME, NAME, TYPE, NAME);
				appendf(meta_builder, "procedure constexpr %.*s operator>>=(      %.*s& a, const %.*s n) { return a = static_cast<%.*s>(static_cast<%.*s>(a) >> n); }\n", NAME, NAME, TYPE, NAME);

				appendf(meta_builder, "global constexpr struct META_%.*s_t { %.*s flag; %.*s value; union { String str; struct { byte PADDING_[offsetof(String, data)]; strlit cstr; }; }; } META_%.*s[] =\n", NAME, NAME, TYPE, NAME);
				appendf(meta_builder, "\t{\n");

				bool32 reported_implicit_member = false;
				FOR_NODES(enumerator.members)
				{
					#define MEMBER PASS_ISTR(it->name)
					if (!reported_implicit_member && !it->definition)
					{
						reported_implicit_member = true;
						printf(":: WaRNING : Flag member (%.*s::%.*s) has implicit definition.\n", NAME, MEMBER);
						report(String("Warning from this meta operation."), &main_tokenizer);
					}

					appendf(meta_builder, "\t\t{ %.*s::%.*s, static_cast<%.*s>(%.*s::%.*s), String(\"%.*s\") }", NAME, MEMBER, TYPE, NAME, MEMBER, MEMBER);
					if (it->next)
					{
						appendf(meta_builder, ",");
					}
					appendf(meta_builder, "\n");
					#undef MEMBER
				}

				appendf(meta_builder, "\t};\n");
				#undef TYPE
				#undef NAME

				appendf(meta_builder, "#pragma clang diagnostic pop\n");

				String meta_data = flush(meta_builder);
				append(meta_builder, String(SRC_DIR));
				append(meta_builder, file_path);

				if (!write(flush(meta_builder), meta_data))
				{
					report(String("Failed to write meta data to file."), &main_tokenizer);
					return 1;
				}
			}
			else
			{
				report(String("Unknown meta operation."), &main_tokenizer);
				return 1;
			}
		}
	}

	return 0;
}
