#define UNICODE true
#define TokenType TokenType_
#include <Windows.h>
#include <ShlObj_core.h>
#include <strsafe.h>
#undef TokenType
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
	if (mbstowcs_s(&wide_dir_path_count, wide_dir_path, dir_path.data, dir_path.size) || wide_dir_path_count != dir_path.size + 1)
	{
		return {};
	}

	if (StringCchCatW(wide_dir_path, ARRAY_CAPACITY(wide_dir_path), L"/*") != S_OK)
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
					if (StringCchCatW(wide_file_path, ARRAY_CAPACITY(wide_file_path), find_data.cFileName) != S_OK)
					{
						return {};
					}

					char* file_path_data = allocate<char>(arena, wcslen(wide_file_path) + 1);
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
									.size = wcslen(wide_file_path),
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

procedure bool32 write(String file_path, String content)
{
	wchar_t wide_file_path[MAX_PATH];
	u64     wide_file_path_count;
	if (mbstowcs_s(&wide_file_path_count, wide_file_path, file_path.data, file_path.size) || wide_file_path_count != file_path.size + 1)
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
	return content.size < (1ull << 32) && WriteFile(handle, content.data, static_cast<DWORD>(content.size), &write_amount, 0) && write_amount == content.size;
}

struct StringBuilderCharBufferNode
{
	StringBuilderCharBufferNode* next;
	u64  count;
	char buffer[4096];
};

struct StringBuilder
{
	MemoryArena*                 arena;
	u64                          size;
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
	u64 index = 0;
	while (index < str.size)
	{
		if (builder->curr->count == ARRAY_CAPACITY(builder->curr->buffer))
		{
			if (!builder->curr->next)
			{
				builder->curr->next = allocate<StringBuilderCharBufferNode>(builder->arena);
			}
			builder->curr  = builder->curr->next;
			*builder->curr = {};
		}

		u64 write_amount = min(str.size - index, ARRAY_CAPACITY(builder->curr->buffer) - builder->curr->count);
		memcpy(builder->curr->buffer + builder->curr->count, str.data + index, write_amount);
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
	i32 APPENDF_WRITE_AMOUNT_ = sprintf_s(APPENDF_BUFFER_, ARRAY_CAPACITY(APPENDF_BUFFER_), (FORMAT), __VA_ARGS__);\
	ASSERT(IN_RANGE(APPENDF_WRITE_AMOUNT_, 0, static_cast<i32>(ARRAY_CAPACITY(APPENDF_BUFFER_))));\
	append((BUILDER), { static_cast<u64>(APPENDF_WRITE_AMOUNT_), APPENDF_BUFFER_ });\
}\
while (false)

procedure String flush(StringBuilder* builder)
{
	char* data = allocate<char>(builder->arena, builder->size);

	u64 write_count = 0;
	FOR_NODES(&builder->head)
	{
		memcpy(data + write_count, it->buffer, it->count);
		write_count += it->count;
	}

	String result = { builder->size, data };

	builder->size       = 0;
	builder->head.count = 0;
	builder->curr       = &builder->head;

	return result;
}

enum struct TokenType : u8
{
	null,
	meta,
	preprocessor,
	reserved,
	identifier,
	number,
	string,
	character
};

enum struct ReservedType : u8
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
			ReservedType type;
		} reserved;
	};
};

struct TokenBufferNode
{
	TokenBufferNode* next;
	TokenBufferNode* prev;
	u64              count;
	Token            buffer[4096];
};

struct Tokenizer
{
	String           file_path;
	u64              file_size;
	char*            file_data;
	i32              curr_index_in_node;
	TokenBufferNode* curr_node;
};

procedure Tokenizer init_tokenizer(String file_path, MemoryArena* arena)
{
	Tokenizer tokenizer =
		{
			.file_path = file_path
		};

	{
		HANDLE handle;
		wchar_t wide_file_path[MAX_PATH];
		u64     wide_file_path_count;
		if (mbstowcs_s(&wide_file_path_count, wide_file_path, file_path.data, file_path.size) || wide_file_path_count != file_path.size + 1)
		{
			return {};
		}

		handle = CreateFileW(wide_file_path, GENERIC_READ, 0, {}, OPEN_EXISTING, 0, {});
		if (handle == INVALID_HANDLE_VALUE)
		{
			return {};
		}
		DEFER { CloseHandle(handle); };

		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(handle, &file_size))
		{
			return {};
		}

		tokenizer.file_size = static_cast<u64>(file_size.QuadPart);
		tokenizer.file_data = allocate<char>(arena, tokenizer.file_size);

		DWORD write_amount;
		if (tokenizer.file_size >= (1ULL << 32) || !ReadFile(handle, tokenizer.file_data, static_cast<DWORD>(tokenizer.file_size), &write_amount, {}) || write_amount != tokenizer.file_size)
		{
			return {};
		}
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
			[&](u64 offset)
			{
				offset = min(offset, static_cast<u64>(tokenizer.file_data + tokenizer.file_size - curr_read));
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
			while (peek_read(0) && IMPLIES(!escaped, peek_read(0) != '\n'))
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
					token.type          = TokenType::reserved;
					token.reserved.type = static_cast<ReservedType>(static_cast<i32>(ReservedType::MULTIBYTE_START) + it_index);
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
			token.type = TokenType::reserved;

			FOR_ELEMS(RESERVED_STRINGS)
			{
				if (curr_read + it->size <= tokenizer.file_data + tokenizer.file_size && String { it->size, curr_read } == *it)
				{
					token.reserved.type = static_cast<ReservedType>(static_cast<i32>(ReservedType::MULTIBYTE_START) + it_index);
					inc(it->size);
					goto BREAK;
				}
			}
			{
				token.reserved.type = static_cast<ReservedType>(peek_read(0));
				inc(1);
			}
			BREAK:;
		}

		if (token.type != TokenType::null)
		{
			if (tokenizer.curr_node->count == ARRAY_CAPACITY(tokenizer.curr_node->buffer))
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

procedure bool32 is_reserved(Token token, ReservedType type)
{
	return token.type == TokenType::reserved && token.reserved.type == type;
}

procedure bool32 is_reserved(Token token, char type)
{
	return token.type == TokenType::reserved && token.reserved.type == static_cast<ReservedType>(type);
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
				ASSERT(tokenizer->curr_node->prev->count == ARRAY_CAPACITY(tokenizer->curr_node->prev->buffer));
				tokenizer->curr_node          = tokenizer->curr_node->prev;
				tokenizer->curr_index_in_node = ARRAY_CAPACITY(tokenizer->curr_node->buffer) - 1;
			}
			else
			{
				tokenizer->curr_index_in_node = 0;
				return {};
			}
		}
		else if (static_cast<u64>(tokenizer->curr_index_in_node) >= tokenizer->curr_node->count)
		{
			if (tokenizer->curr_node->next)
			{
				ASSERT(tokenizer->curr_node->count == ARRAY_CAPACITY(tokenizer->curr_node->buffer));
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
				line.size = static_cast<u64>(curr_token.text.data + curr_token.text.size - line.data);
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

enum struct TypeContainerType : u8
{
	null,
	a_struct,
	a_union
};

enum struct ASTType : u8
{
	null,
	a_enum,
	type_atom,
	type_array,
	type_container,
	declaration
};

struct ASTNode;
struct AST
{
	ASTType type;
	String  meta;
	union
	{
		struct
		{
			String   name;
			AST*     underlying_type;
			ASTNode* members;
		} a_enum;

		struct
		{
			String name;
		} type_atom;

		struct
		{
			AST*       underlying_type;
			TokenNode* capacity;
		} type_array;

		struct
		{
			TypeContainerType type;
			String            name;
			ASTNode*          declarations;
		} type_container;

		struct
		{
			AST*   underlying_type;
			String name;
		} declaration;
	};
};

struct ASTNode
{
	ASTNode* next;
	AST*     ast;
};

procedure AST* init_ast(Tokenizer* tokenizer, MemoryArena* arena)
{
	Token token = shift(tokenizer, 0);

	if (is_reserved(token, ReservedType::a_enum))
	{
		AST* ast = allocate<AST>(arena);
		*ast = { .type = ASTType::a_enum };

		token = shift(tokenizer, 1);
		if (token.type != TokenType::identifier)
		{
			report(String("Expected an identifier for the enum name."), tokenizer);
			return {};
		}

		ast->a_enum.name = token.text;

		token = shift(tokenizer, 1); // @TODO@ Inferred underlying type.
		if (!is_reserved(token, ':'))
		{
			report(String("Expected a colon to denote underlying type."), tokenizer);
			return {};
		}

		token = shift(tokenizer, 1);
		if (token.type != TokenType::identifier)
		{
			report(String("Expected an identifier that would be the underlying type."), tokenizer);
			return {};
		}

		ast->a_enum.underlying_type  = allocate<AST>(arena);
		*ast->a_enum.underlying_type =
			{
				.type      = ASTType::type_atom,
				.type_atom = { .name = token.text }
			};

		token = shift(tokenizer, 1);
		if (token.type == TokenType::meta)
		{
			ast->meta = token.meta.info;
			token     = shift(tokenizer, 1);
		}
		if (!is_reserved(token, '{'))
		{
			report(String("Expected an opening curly brace."), tokenizer);
			return {};
		}

		token = shift(tokenizer, 1);
		ASTNode** nil = &ast->a_enum.members;
		while (true)
		{
			token = shift(tokenizer, 0);
			if (is_reserved(token, '}'))
			{
				break;
			}
			else if (token.type == TokenType::identifier)
			{
				*nil         = allocate<ASTNode>(arena);
				**nil        = { .ast = allocate<AST>(arena) };
				*(*nil)->ast =
					{
						.type        = ASTType::declaration,
						.declaration = { .name = token.text }
					};

				token = shift(tokenizer, 1);
				if (is_reserved(token, ','))
				{
					token = shift(tokenizer, 1);
				}
				if (token.type == TokenType::meta)
				{
					(*nil)->ast->meta = token.meta.info;
					token = shift(tokenizer, 1);
				}

				nil = &(*nil)->next;
			}
			else
			{
				report(String("Expected an identifier for enum member."), tokenizer);
				return {};
			}
		}

		token = shift(tokenizer, 1);
		if (!is_reserved(token, ';'))
		{
			report(String("Expected semicolon."), tokenizer);
			return {};
		}

		token = shift(tokenizer, 1);

		return ast;
	}
	else if (is_reserved(token, ReservedType::a_struct) || is_reserved(token, ReservedType::a_union))
	{
		AST* ast = allocate<AST>(arena);
		*ast =
			{
				.type           = ASTType::type_container,
				.type_container =
					{
						.type =
							is_reserved(token, ReservedType::a_struct)
								? TypeContainerType::a_struct
								: TypeContainerType::a_union
					}
			};

		Tokenizer old_tokenizer = *tokenizer;

		token = shift(tokenizer, 1);
		if (token.type == TokenType::identifier)
		{
			ast->type_container.name = token.text;
			token = shift(tokenizer, 1);
		}

		if (!is_reserved(token, '{'))
		{
			report(String("Expected `{`."), tokenizer);
			return {};
		}

		ASTNode** nil = &ast->type_container.declarations;
		for (token = shift(tokenizer, 1); !is_reserved(token, '}'); token = shift(tokenizer, 0))
		{
			AST* sub_ast = init_ast(tokenizer, arena);
			if (sub_ast)
			{
				if (sub_ast->type != ASTType::declaration)
				{
					report(String("Expected declaration."), tokenizer);
					return {};
				}

				*nil  = allocate<ASTNode>(arena);
				**nil = { .ast = sub_ast };
				nil   = &(*nil)->next;
			}
			else
			{
				report(String("Failed to parse for decalarations."), &old_tokenizer);
				return {};
			}
		}

		token = shift(tokenizer, 1);
		if (token.type == TokenType::identifier)
		{
			AST* declaration = allocate<AST>(arena);
			*declaration =
				{
					.type        = ASTType::declaration,
					.declaration =
						{
							.underlying_type = ast,
							.name            = token.text
						}
				};
			ast   = declaration;
			token = shift(tokenizer, 1);
		}
		if (!is_reserved(token, ';'))
		{
			report(String("Expected semicolon."), tokenizer);
			return {};
		}

		token = shift(tokenizer, 1);
		if (token.type == TokenType::meta)
		{
			ast->meta = token.meta.info;
			token = shift(tokenizer, 1);
		}

		return ast;
	}
	else if (token.type == TokenType::identifier)
	{
		AST* ast = allocate<AST>(arena);
		*ast =
			{
				.type        = ASTType::declaration,
				.declaration = { .underlying_type = allocate<AST>(arena) }
			};
		*ast->declaration.underlying_type =
			{
				.type      = ASTType::type_atom,
				.type_atom =
					{
						.name = token.text
					}
			};

		token = shift(tokenizer, 1);
		if (token.type != TokenType::identifier)
		{
			report(String("Expected name of declaration."), tokenizer);
			return {};
		}

		ast->declaration.name = token.text;

		token = shift(tokenizer, 1);
		if (is_reserved(token, '['))
		{
			{
				AST* array = allocate<AST>(arena);
				*array =
					{
						.type       = ASTType::type_array,
						.type_array = { .underlying_type = ast->declaration.underlying_type }
					};
				ast->declaration.underlying_type = array;
			}

			TokenNode** nil = &ast->declaration.underlying_type->type_array.capacity;
			for (token = shift(tokenizer, 1); !is_reserved(token, ']'); token = shift(tokenizer, 1))
			{
				TokenNode* node = allocate<TokenNode>(arena);
				*node = { .token = token };
				*nil  = node;
				nil   = &(*nil)->next;
			}
			token = shift(tokenizer, 1);
		}
		if (!is_reserved(token, ';'))
		{
			report(String("Expected semicolon."), tokenizer);
			return {};
		}

		token = shift(tokenizer, 1);
		if (token.type == TokenType::meta)
		{
			ast->meta = token.meta.info;
			token = shift(tokenizer, 1);
		}

		return ast;
	}
	else
	{
		report(String("Unknown handling of token."), tokenizer);
		return {};
	}
}

int main()
{
	MemoryArena main_arena = {};
	main_arena.size = MEBIBYTES_OF(1);
	main_arena.data = reinterpret_cast<byte*>(malloc(main_arena.size));
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
			if (main_token.type == TokenType::preprocessor)
			{
				String file_path = main_token.text;
				ASSERT(file_path.size && file_path.data[0] == '#');

				file_path = ltrim_whitespace(ltrim(file_path, 1));

				if (!starts_with(file_path, String("include")))
				{
					continue;
				}

				file_path = trim(ltrim_whitespace(ltrim(file_path, String("include").size)), 1, 1);

				if (!starts_with(file_path, String("META/")))
				{
					continue;
				}

				String meta_operation = ltrim(file_path, String("META/").size);
				String file_name      = meta_operation;
				FOR_STR(meta_operation)
				{
					if (*it == '/')
					{
						meta_operation.size = it_index;
						file_name           = ltrim(file_name, it_index + 1);
						break;
					}
				}
				FOR_STR(file_name)
				{
					if (*it == '.')
					{
						file_name.size = it_index;
						break;
					}
				}

				DEFER_ARENA_RESET(&main_arena);

				if (meta_operation == String("enum"))
				{
					Tokenizer meta_tokenizer = main_tokenizer;
					Token     meta_token     = shift(&meta_tokenizer, 0);

					while (true)
					{
						meta_token = shift(&meta_tokenizer, -1);

						if (is_reserved(meta_token, ReservedType::a_enum))
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

					AST* ast = init_ast(&meta_tokenizer, &main_arena);
					if (!ast)
					{
						report(String("Failed to parse the AST for the meta operation."), &main_tokenizer);
						return 1;
					}

					ASSERT(ast->type == ASTType::a_enum);
					ASSERT(ast->a_enum.underlying_type && ast->a_enum.underlying_type->type == ASTType::type_atom);

					StringBuilder* meta_builder = init_string_builder(&main_arena);

					append(meta_builder, String("global inline constexpr struct META_"));
					append(meta_builder, ast->a_enum.name);
					append(meta_builder, String("_t { "));
					append(meta_builder, ast->a_enum.name);
					append(meta_builder, String(" enumerator; "));
					append(meta_builder, ast->a_enum.underlying_type->type_atom.name);
					append(meta_builder, String(" value; union { String str; struct { byte PADDING_[offsetof(String, data)]; strlit cstr; }; }; "));

					if (+ast->meta)
					{
						append(meta_builder, ast->meta);
						append(meta_builder, String(" "));
					}

					append(meta_builder, String("} META_"));
					append(meta_builder, ast->a_enum.name);
					append(meta_builder, String("[] =\n\t{\n"));

					FOR_NODES(ast->a_enum.members)
					{
						append(meta_builder, String("\t\t{ "));
						append(meta_builder, ast->a_enum.name);
						append(meta_builder, String("::"));
						append(meta_builder, it->ast->declaration.name);
						append(meta_builder, String(", static_cast<"));
						append(meta_builder, ast->a_enum.underlying_type->type_atom.name);
						append(meta_builder, String(">("));
						append(meta_builder, ast->a_enum.name);
						append(meta_builder, String("::"));
						append(meta_builder, it->ast->declaration.name);
						append(meta_builder, String("), String(\""));
						append(meta_builder, it->ast->declaration.name);
						append(meta_builder, String("\")"));

						if (+it->ast->meta)
						{
							if (+ast->meta)
							{
								append(meta_builder, String(", "));
								append(meta_builder, it->ast->meta);
							}
							else
							{
								report(String("An enum member has a meta tag when the enum declaration does not."), &main_tokenizer);
								return 1;
							}
						}

						append(meta_builder, String(" }"));
						if (it->next)
						{
							append(meta_builder, String(","));
						}
						append(meta_builder, String("\n"));
					}

					append(meta_builder, String("\t};\n"));

					String meta_data = flush(meta_builder);
					append(meta_builder, String(SRC_DIR));
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
					if (!is_reserved(meta_token, ';'))
					{
						report(String("Expected semicolon to denote the end of a union declaration."), &meta_tokenizer);
						return 1;
					}

					meta_token = shift(&meta_tokenizer, -1);
					if (!is_reserved(meta_token, '}'))
					{
						report(String("Expected a closing curly brace to denote the end of a union declaration."), &meta_tokenizer);
						return 1;
					}

					for (i32 depth= 1; depth;)
					{
						meta_token = shift(&meta_tokenizer, -1);

						if (meta_token.type == TokenType::null)
						{
							report(String("Failed to find the corresponding union for the meta oepration."), &main_tokenizer);
							return 1;
						}
						else if (is_reserved(meta_token, '{'))
						{
							depth -= 1;
							if (depth == 0)
							{
								meta_token = shift(&meta_tokenizer, -1);
								if (!is_reserved(meta_token, ReservedType::a_union))
								{
									report(String("Expected keyword `union` here."), &meta_tokenizer);
									report(String("Failed to do meta operation."), &main_tokenizer);
									return 1;
								}
							}
						}
						else if (is_reserved(meta_token, '}'))
						{
							depth += 1;
						}
					}

					AST* ast = init_ast(&meta_tokenizer, &main_arena);
					if (!ast)
					{
						report(String("Failed to parse the AST for the meta operation."), &main_tokenizer);
						return 1;
					}
					if
					(
						ast->type != ASTType::type_container || ast->type_container.type != TypeContainerType::a_union ||
						!ast->type_container.declarations       || ast->type_container.declarations->ast->declaration.underlying_type->type != ASTType::type_container || ast->type_container.declarations->ast->declaration.underlying_type->type_container.type != TypeContainerType::a_struct ||
						!ast->type_container.declarations->next || ast->type_container.declarations->next->ast->declaration.underlying_type->type != ASTType::type_array
					)
					{
						report(String("Invalid layout for meta operation."), &main_tokenizer);
						return 1;
					}

					StringBuilder* meta_builder  = init_string_builder(&main_arena);
					StringBuilder* defer_builder = init_string_builder(&main_arena);

					append(meta_builder, String("global inline constexpr String META_"));
					append(meta_builder, file_name);
					append(meta_builder, String("[] =\n\t{\n"));

					FOR_NODES(declaration, ast->type_container.declarations->ast->declaration.underlying_type->type_container.declarations)
					{
						if (!declaration->ast->meta.data)
						{
							report(String("Not all assets have a meta tag."), &main_tokenizer);
							return 1;
						}

						append(meta_builder, String("\t\t"));

						i32    asset_path_count = 0;
						for (String remaining = ltrim_whitespace(declaration->ast->meta); remaining.size; remaining = ltrim_whitespace(remaining))
						{
							asset_path_count += 1;

							String asset_path = remaining;
							FOR_STR(asset_path)
							{
								if (*it == ',')
								{
									asset_path.size = it_index;
									break;
								}
							}

							remaining  = ltrim_whitespace(ltrim(remaining, asset_path.size + 1));
							asset_path = rtrim_whitespace(asset_path);

							append(meta_builder, String("String(DATA_DIR \""));
							append(meta_builder, asset_path);
							append(meta_builder, String("\")"));

							if (remaining.size || declaration->next)
							{
								append(meta_builder, String(", "));
							}
						}
						append(meta_builder , String("\n"));
						append (defer_builder, String("static_assert(sizeof("));
						append (defer_builder, ast->type_container.declarations->ast->declaration.name);
						append (defer_builder, String("."));
						append (defer_builder, declaration->ast->declaration.name);
						append (defer_builder, String(") / sizeof("));
						append (defer_builder, ast->type_container.declarations->next->ast->declaration.underlying_type->type_array.underlying_type->type_atom.name);
						append (defer_builder, String(") == "));
						appendf(defer_builder, "%d", asset_path_count);
						append (defer_builder, String(", \"("));
						append (defer_builder, ast->type_container.declarations->ast->declaration.name);
						append (defer_builder, String("."));
						append (defer_builder, declaration->ast->declaration.name);
						append (defer_builder, String(")("));
						appendf(defer_builder, "%d", main_token.line_index + 1);
						append (defer_builder, String(") :: The amount of assets and file paths are not the same.\");\n"));
					}

					append(meta_builder, String("\t};\n"));
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
				else if (meta_operation == String("variant"))
				{
					struct NamePairNode
					{
						NamePairNode* next;
						String        semantic;
						String        container;
					};

					Tokenizer      meta_tokenizer = main_tokenizer;
					StringBuilder* meta_builder   = init_string_builder(&main_arena);
					NamePairNode*  name_pairs     = {};
					Token          meta_token     = shift(&meta_tokenizer, -1);
					for (; meta_token.type != TokenType::null && meta_token.text.data != main_token.text.data; meta_token = shift(&meta_tokenizer, -1))
					{
						if (meta_token.type == TokenType::meta && starts_with(meta_token.meta.info, file_name))
						{
							String semantic_name = ltrim(meta_token.meta.info, file_name.size);
							if (semantic_name.size == 0 || !is_whitespace(semantic_name.data[0]))
							{
								continue;
							}
							semantic_name = trim_whitespace(semantic_name);
							FOR_STR(semantic_name)
							{
								if (*it != '_' && !is_alpha(*it) && IMPLIES(is_digit(*it), it_index == 0))
								{
									report(String("Invalid identifier."), &meta_tokenizer);
									report(String("Failed to do meta operation."), &main_tokenizer);
									return 1;
								}
							}

							meta_token = shift(&meta_tokenizer, -1);
							if (meta_token.type != TokenType::identifier)
							{
								report(String("Expected identifier."), &meta_tokenizer);
								report(String("Failed to do meta operation."), &main_tokenizer);
								return 1;
							}

							String container_name = meta_token.text;

							meta_token = shift(&meta_tokenizer, -1);
							if (!is_reserved(meta_token, ReservedType::a_struct))
							{
								report(String("Expected `struct`."), &meta_tokenizer);
								report(String("Failed to do meta operation."), &main_tokenizer);
								return 1;
							}

							NamePairNode* node = allocate<NamePairNode>(&main_arena);
							*node =
								{
									.next      = name_pairs,
									.semantic  = semantic_name,
									.container = container_name
								};
							name_pairs = node;
						}
					}

					append(meta_builder, String("#pragma clang diagnostic push\n#pragma clang diagnostic ignored \"-Wunused-function\"\n"));

					append(meta_builder, String("enum struct "));
					append(meta_builder, file_name);
					append(meta_builder, String("Type : u8\n{\n\tnull"));
					FOR_NODES(name_pairs)
					{
						append(meta_builder, String(",\n\t"));
						append(meta_builder, it->semantic);
					}
					append(meta_builder, String("\n};\n\nstruct "));
					append(meta_builder, file_name);
					append(meta_builder, String("\n{\n\t"));
					append(meta_builder, file_name);
					append(meta_builder, String("Type type;\n\tunion\n\t{\n"));
					FOR_NODES(name_pairs)
					{
						append(meta_builder, String("\t\t"));
						append(meta_builder, it->container);
						append(meta_builder, String(" "));
						append(meta_builder, it->semantic);
						append(meta_builder, String(";\n"));
					}
					append(meta_builder, String("\t};\n};\n\nstruct "));
					append(meta_builder, file_name);
					append(meta_builder, String("Ptr\n{\n\t"));
					append(meta_builder, file_name);
					append(meta_builder, String("Type type;\n\tunion\n\t{\n"));
					FOR_NODES(name_pairs)
					{
						append(meta_builder, String("\t\t"));
						append(meta_builder, it->container);
						append(meta_builder, String("* "));
						append(meta_builder, it->semantic);
						append(meta_builder, String(";\n"));
					}
					append(meta_builder, String("\t};\n};\n\n"));

					FOR_NODES(name_pairs)
					{
						appendf(meta_builder, "procedure %.*s    widen(const %.*s& x) { return { %.*sType::%.*s, { .%.*s = x } }; }\n", PASS_ISTR(file_name), PASS_ISTR(it->container), PASS_ISTR(file_name), PASS_ISTR(it->semantic), PASS_ISTR(it->semantic));
						appendf(meta_builder, "procedure %.*sPtr widen(      %.*s* x) { return { %.*sType::%.*s, { .%.*s = x } }; }\n", PASS_ISTR(file_name), PASS_ISTR(it->container), PASS_ISTR(file_name), PASS_ISTR(it->semantic), PASS_ISTR(it->semantic));
					}

					append(meta_builder, String("#pragma clang diagnostic pop\n"));

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
	}


	return 0;
}
