#undef UNICODE
#define UNICODE true
#pragma warning(push)
#pragma warning(disable : 5039 4820 4061 4365)
#include <windows.h>
#include <shlobj_core.h>
#pragma warning(pop)
#include <stdio.h>
#include <stdlib.h>
#include "unified.h"
#undef  ASSERT
#define ASSERT(EXPRESSION)\
do\
{\
	if (!(EXPRESSION))\
	{\
		printf(__FILE__ " (`" MACRO_STRINGIFY(__LINE__) "`) :: Internal error. Failed assertion. :: `"  #EXPRESSION "`.\n");\
		DEBUG_printf(__FILE__ " (`" MACRO_STRINGIFY(__LINE__) "`) :: Internal error. Failed assertion. :: `"  #EXPRESSION "`.\n");\
		*reinterpret_cast<i32*>(0) = 0;\
		exit(1);\
	}\
}\
while (false)

#define malloc(X) (DEBUG_ALLOCATION_COUNT += 1, malloc(X))
#define free(X)   (DEBUG_ALLOCATION_COUNT -= 1, free(X))
global i32 DEBUG_ALLOCATION_COUNT = 0;

struct File
{
	HANDLE handle;
};

internal File init_file(String file_path)
{
	File file = { .handle = INVALID_HANDLE_VALUE };

	char buffer[256];
	i32 length = sprintf_s(buffer, SRC_DIR "%.*s", PASS_STR(file_path));
	ASSERT(length == static_cast<i32>(sizeof(SRC_DIR) - 1 + file_path.size));

	size_t newsize = length + 1;

	wchar_t wcstring[256];

	size_t convertedChars = 0;
	errno_t result = mbstowcs_s(&convertedChars, wcstring, newsize, buffer, _TRUNCATE);
	ASSERT(result != EINVAL);

	{
		wchar_t dir[256];

		i32 last_slash = 0;
		FOR_ELEMS_REV(c, wcstring, newsize)
		{
			// @TODO@ Backslashes?
			if (*c == L'/')
			{
				last_slash = c_index;
				break;
			}
		}
		FOR_ELEMS(c, wcstring, last_slash)
		{
			if (*c == L'/')
			{
				dir[c_index] = L'\\';
			}
			else
			{
				dir[c_index] = *c;
			}
		}
		dir[last_slash] = L'\0';

		i32 result = SHCreateDirectoryExW(0, dir, 0);
		if (result != ERROR_SUCCESS && result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS)
		{
			ASSERT(!"Failed to make directory");
		}
	}

	file.handle = CreateFileW(wcstring, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (file.handle == INVALID_HANDLE_VALUE)
	{
		ASSERT(!"Failed to make file");
	}

	return file;
}

internal void deinit_file(File* file)
{
	if (!CloseHandle(file->handle))
	{
		ASSERT(!"Failed to close");
	}
}

internal bool32 write(File* file, String str)
{
	DWORD bytes_written;
	return file->handle && file->handle != INVALID_HANDLE_VALUE && WriteFile(file->handle, str.data, str.size, &bytes_written, 0) && bytes_written == str.size;
}

struct StringBuilder
{
	i32  count;
	char buffer[4096];
};

template <typename... ARGUMENTS>
internal void append(StringBuilder* builder, strlit format, ARGUMENTS... arguments)
{
	builder->count += sprintf_s(builder->buffer + builder->count, ARRAY_CAPACITY(builder->buffer) - builder->count, format, arguments...);
}

internal String build(StringBuilder* builder)
{
	return { builder->count, builder->buffer };
}

enum struct TokenKind : u8
{
	null,

	// @NOTE@ ASCII characters 0-127.

	meta = 128,
	preprocessor,
	identifier,
	number,
	string,
	character
};

// @TODO@ Compact this?
struct Token
{
	TokenKind kind;
	String    str;
};

struct TokenNode
{
	TokenNode* next;
	Token      token;
};

struct TokenBufferNode
{
	TokenBufferNode* prev;
	TokenBufferNode* next;
	i32              count;
	Token            buffer[4096];
};

struct Tokenizer
{
	i32              curr_token_index;
	TokenBufferNode* curr_token_buffer_node;
	i32              file_size;
	char*            file_data;
};

internal Tokenizer init_tokenizer(strlit file_path)
{
	Tokenizer tokenizer = {};

	FILE* file = 0;
	ASSERT(fopen_s(&file, file_path, "rb") != EINVAL); // @TODO@ Handle error.
	ASSERT(file);
	DEFER { fclose(file); };

	fseek(file, 0, SEEK_END);
	tokenizer.file_size = ftell(file);
	tokenizer.file_data = reinterpret_cast<char*>(malloc(static_cast<size_t>(tokenizer.file_size)));
	fseek(file, 0, SEEK_SET);
	fread(tokenizer.file_data, sizeof(char), static_cast<size_t>(tokenizer.file_size), file);

	tokenizer.curr_token_buffer_node  = reinterpret_cast<TokenBufferNode*>(malloc(sizeof(TokenBufferNode)));
	*tokenizer.curr_token_buffer_node = {};

	{
		TokenBufferNode* curr_node           = tokenizer.curr_token_buffer_node;
		bool32           can_be_preprocessor = true;
		char*            curr_read           = tokenizer.file_data;
		char*            read_eof            = tokenizer.file_data + tokenizer.file_size;
		while (curr_read < read_eof)
		{
			Token token = {};

			switch (curr_read[0])
			{
				case ' ':
				case '\t':
				case '\r':
				{
					curr_read += 1;
				} break;

				case '\n':
				{
					can_be_preprocessor  = true;
					curr_read           += 1;
				} break;

				default:
				{
					if (curr_read + 1 < read_eof && curr_read[0] == '/' && curr_read[1] == '/')
					{
						curr_read += 2;

						while (curr_read < read_eof && curr_read[0] == ' ')
						{
							curr_read += 1;
						}

						constexpr String TAG = STR_OF("@META@");
						if (curr_read + TAG.size <= read_eof && memcmp(curr_read, TAG.data, TAG.size) == 0)
						{
							curr_read += TAG.size;

							while (curr_read < read_eof && curr_read[0] == ' ')
							{
								curr_read += 1;
							}

							token.kind     = TokenKind::meta;
							token.str.data = curr_read;

							while (curr_read + token.str.size < read_eof && curr_read[token.str.size] != '\r' && curr_read[token.str.size] != '\n')
							{
								token.str.size += 1;
							}
							curr_read += token.str.size;
						}
						else
						{
							while (curr_read < read_eof && curr_read[0] != '\n')
							{
								curr_read += 1;
							}
						}
					}
					else if (curr_read + 1 < read_eof && curr_read[0] == '/' && curr_read[1] == '*')
					{
						curr_read += 4;
						while (curr_read < read_eof && !(curr_read[-2] == '*' && curr_read[-1] == '/'))
						{
							curr_read += 1;
						}
					}
					else
					{
						token.str.data = curr_read;

						if (curr_read[0] == '#' && can_be_preprocessor)
						{
							token.kind      = TokenKind::preprocessor;
							token.str.data += 1;
							curr_read      += 1;

							bool32 escaped = false;
							while
							(
								curr_read + token.str.size < read_eof
								&& IMPLIES(curr_read[token.str.size] == '\r' || curr_read[token.str.size] == '\n', escaped)
								&& IMPLIES(curr_read + token.str.size + 1 < read_eof, !(curr_read[token.str.size] == '/' && curr_read[token.str.size + 1] == '/'))
								&& IMPLIES(curr_read + token.str.size + 1 < read_eof, !(curr_read[token.str.size] == '/' && curr_read[token.str.size + 1] == '*'))
							)
							{
								if (curr_read[token.str.size] == '\\')
								{
									escaped = !escaped;
								}
								else if (curr_read[token.str.size] != '\r')
								{
									escaped = false;
								}

								token.str.size += 1;
							}
						}
						else
						{
							can_be_preprocessor = false;

							if (is_alpha(curr_read[0]))
							{
								token.kind = TokenKind::identifier;

								while
								(
									curr_read + token.str.size < read_eof &&
									(
										curr_read[token.str.size] == '_'
										|| is_alpha(curr_read[token.str.size])
										|| is_digit(curr_read[token.str.size])
									)
								)
								{
									token.str.size += 1;
								}
							}
							else if (is_digit(curr_read[0]) || curr_read[0] == '.' && curr_read + 1 < read_eof && is_digit(curr_read[1]))
							{
								token.kind = TokenKind::number;

								while
								(
									curr_read + token.str.size < read_eof &&
									(
										is_digit(curr_read[token.str.size])
										|| is_alpha(curr_read[token.str.size]) // @TODO@ Suffixes.
										|| curr_read[token.str.size] == '.'
										|| curr_read[token.str.size] == '\''
									)
								)
								{
									token.str.size += 1;
								}
							}
							else if (curr_read[0] == '"')
							{
								token.kind      = TokenKind::string;
								token.str.data += 1;
								curr_read      += 1;

								bool32 escaped = false;
								while (curr_read + token.str.size < read_eof && IMPLIES(curr_read[token.str.size] == '"', escaped))
								{
									if (curr_read[token.str.size] == '\\')
									{
										escaped = !escaped;
									}
									else
									{
										escaped = false;
									}

									token.str.size += 1;
								}
								curr_read += 1;
							}
							else if (curr_read[0] == '\'')
							{
								token.kind      = TokenKind::character;
								token.str.size  = 1;
								token.str.data += 1;
								curr_read      += 1;

								if (curr_read < read_eof && curr_read[0] == '\\')
								{
									token.str.data += 1;
									curr_read      += 1;
								}

								curr_read += 1;
							}
							else
							{
								token.kind      = static_cast<TokenKind>(curr_read[0]),
								token.str.size += 1;
							}
						}
						curr_read += token.str.size; // @TODO@ Distribute this into all the cases.
					}
				} break;
			}

			if (token.kind != TokenKind::null)
			{
				if (!IN_RANGE(curr_node->count, 0, ARRAY_CAPACITY(curr_node->buffer)))
				{
					curr_node->next  = reinterpret_cast<TokenBufferNode*>(malloc(sizeof(TokenBufferNode)));
					*curr_node->next = { .prev = curr_node };
					curr_node        = curr_node->next;
				}

				curr_node->buffer[curr_node->count] = token;
				curr_node->count += 1;
			}
		}
	}

	return tokenizer;
}

internal void deinit_tokenizer(Tokenizer* tokenizer)
{
	free(tokenizer->file_data);

	TokenBufferNode* node = tokenizer->curr_token_buffer_node;
	ASSERT(node);
	while (node->prev)
	{
		node = node->prev;
	}
	while (node)
	{
		TokenBufferNode* tail = node->next;
		free(node);
		node = tail;
	}
}

internal Token shift(Tokenizer* tokenizer, i32 offset)
{
	ASSERT(tokenizer->curr_token_buffer_node);
	ASSERT(IFF(tokenizer->curr_token_buffer_node->count == ARRAY_CAPACITY(tokenizer->curr_token_buffer_node->buffer), tokenizer->curr_token_buffer_node->next));

	i32 steps = offset;
	while (true)
	{
		if (tokenizer->curr_token_index < 0)
		{
			if (tokenizer->curr_token_buffer_node->prev)
			{
				tokenizer->curr_token_buffer_node  = tokenizer->curr_token_buffer_node->prev;
				tokenizer->curr_token_index = tokenizer->curr_token_buffer_node->count - 1;
			}
			else
			{
				tokenizer->curr_token_index = 0;
				return {};
			}
		}
		else if (tokenizer->curr_token_index >= tokenizer->curr_token_buffer_node->count)
		{
			if (tokenizer->curr_token_buffer_node->next)
			{
				tokenizer->curr_token_buffer_node  = tokenizer->curr_token_buffer_node->next;
				tokenizer->curr_token_index = 0;
			}
			else
			{
				tokenizer->curr_token_index = tokenizer->curr_token_buffer_node->count;
				return {};
			}
		}

		ASSERT(IN_RANGE(tokenizer->curr_token_index, 0, tokenizer->curr_token_buffer_node->count));

		if (steps)
		{
			tokenizer->curr_token_index += sign(offset);
			steps -= sign(offset);
		}
		else
		{
			return tokenizer->curr_token_buffer_node->buffer[tokenizer->curr_token_index];
		}
	}
}

internal Token peek(Tokenizer tokenizer, i32 offset = 0)
{
	return shift(&tokenizer, offset);
}

enum struct ContainerType : u8
{
	null,
	a_struct,
	a_union
};

enum struct EnumType : u8
{
	enum_unscoped,
	enum_struct
};

enum struct ASTType : u8
{
	null,
	root_type,
	array,
	container,
	enumerator,
	declaration,
	tokens
};

struct AST;
struct ASTNode
{
	ASTNode* next;
	AST*     ast;
};
struct AST
{
	ASTType type;
	String  meta;
	union
	{
		struct
		{
			String name;
		} root_type;

		struct
		{
			AST* underlying_type;
			AST* capacity;
		} array;

		struct
		{
			ContainerType type;
			String        name;
			ASTNode*      declarations;
		} container;

		struct
		{
			EnumType  type;
			String    name;
			AST*      underlying_type;
			ASTNode*  declarations;
		} enumerator;

		struct
		{
			String name;
			AST*   underlying_type;
			AST*   assignment;
		} declaration;

		struct
		{
			TokenNode* node;
		} tokens;
	};
};

internal AST* init_ast(Tokenizer* tokenizer)
{
	lambda init_single_ast_node =
		[](AST* ast)
		{
			ASTNode* node = reinterpret_cast<ASTNode*>(malloc(sizeof(ASTNode)));
			*node = {};
			node->ast = ast;
			return node;
		};

	lambda init_single_token_node =
		[](Token token)
		{
			TokenNode* node = reinterpret_cast<TokenNode*>(malloc(sizeof(TokenNode)));
			*node = {};
			node->token = token;
			return node;
		};

	Token token = peek(*tokenizer);

	AST* ast = reinterpret_cast<AST*>(malloc(sizeof(AST)));
	*ast = {};

	if (token.kind == TokenKind::identifier && (token.str == STR_OF("struct") || token.str == STR_OF("union")))
	{
		ast->type           = ASTType::container;
		ast->container.type =
			token.str == STR_OF("struct")
				? ContainerType::a_struct
				: ContainerType::a_union;

		ast->container.type = ContainerType::a_union;

		token = shift(tokenizer, 1);

		if (token.kind == TokenKind::identifier)
		{
			ast->container.name = token.str;
			token = shift(tokenizer, 1);
		}

		ASSERT(token.kind == static_cast<TokenKind>('{'));

		token = shift(tokenizer, 1);

		ASTNode** nil = &ast->container.declarations;
		while (true)
		{
			AST* sub_ast = init_ast(tokenizer);
			if (sub_ast)
			{
				ASSERT(sub_ast->type == ASTType::declaration);
				*nil = init_single_ast_node(sub_ast);
				nil  = &(*nil)->next;

				token = peek(*tokenizer);
				ASSERT(token.kind == static_cast<TokenKind>(';'));

				token = shift(tokenizer, 1);

				if (token.kind == TokenKind::meta)
				{
					sub_ast->meta = token.str;
					token = shift(tokenizer, 1);
				}
			}
			else
			{
				break;
			}
		}

		token = peek(*tokenizer, 0);
		ASSERT(token.kind == static_cast<TokenKind>('}'));

		token = shift(tokenizer, 1);
		if (token.kind == TokenKind::identifier)
		{
			AST* declaration = reinterpret_cast<AST*>(malloc(sizeof(AST)));
			*declaration = {};
			declaration->type                        = ASTType::declaration;
			declaration->declaration.name            = token.str;
			declaration->declaration.underlying_type = ast;

			ast   = declaration;
			token = shift(tokenizer, 1);
		}

		// @TODO@ Array of inlined structs.

		return ast;
	}
	else if (token.kind == TokenKind::identifier && token.str == STR_OF("enum"))
	{ // @TODO@ Consider all forms of enums.
		ast->type = ASTType::enumerator;

		token = shift(tokenizer, 1);
		ASSERT(token.kind == TokenKind::identifier);

		if (token.str == STR_OF("struct"))
		{
			ast->enumerator.type = EnumType::enum_struct;
			token = shift(tokenizer, 1);
			ASSERT(token.kind == TokenKind::identifier);
		}

		ast->enumerator.name = token.str;

		token = shift(tokenizer, 1);
		ASSERT(token.kind == static_cast<TokenKind>(':'));

		token = shift(tokenizer, 1);
		ASSERT(token.kind == TokenKind::identifier);
		AST* underlying_type = reinterpret_cast<AST*>(malloc(sizeof(AST)));
		*underlying_type = {};
		underlying_type->type           = ASTType::root_type;
		underlying_type->root_type.name = token.str;
		ast->enumerator.underlying_type = underlying_type;

		token = shift(tokenizer, 1);
		if (token.kind == TokenKind::meta)
		{
			ast->meta = token.str;
			token = shift(tokenizer, 1);
		}
		ASSERT(token.kind == static_cast<TokenKind>('{'));

		token = shift(tokenizer, 1);
		ASTNode** nil = &ast->enumerator.declarations;
		while (true)
		{
			if (token.kind == TokenKind::identifier)
			{
				AST* declaration = reinterpret_cast<AST*>(malloc(sizeof(AST)));
				*declaration = {};
				declaration->type             = ASTType::declaration;
				declaration->declaration.name = token.str;

				token = shift(tokenizer, 1);
				if (token.kind == static_cast<TokenKind>('='))
				{
					ASSERT("Enum assignments are not handled yet."); // @TODO@
				}
				if (token.kind == static_cast<TokenKind>(','))
				{
					token = shift(tokenizer, 1);
				}
				if (token.kind == TokenKind::meta)
				{
					declaration->meta = token.str;
					token = shift(tokenizer, 1);
				}

				*nil = init_single_ast_node(declaration);
				nil  = &(*nil)->next;
			}
			else if (token.kind == static_cast<TokenKind>('}'))
			{
				break;
			}
			else
			{
				ASSERT(!"Unexpected token.");
			}
		}

		// @TODO@ Enum declaration.
		token = shift(tokenizer, 1);
		ASSERT(token.kind == static_cast<TokenKind>(';'));

		return ast;
	}
	else if (token.kind == TokenKind::identifier && peek(*tokenizer, 1).kind == TokenKind::identifier)
	{
		AST* underlying_type = reinterpret_cast<AST*>(malloc(sizeof(AST)));
		*underlying_type = {};
		underlying_type->type           = ASTType::root_type;
		underlying_type->root_type.name = token.str;

		token = shift(tokenizer, 1);
		ast->type                        = ASTType::declaration;
		ast->declaration.name            = token.str;
		ast->declaration.underlying_type = underlying_type;

		token = shift(tokenizer, 1);

		if (token.kind == static_cast<TokenKind>('['))
		{
			AST* array = reinterpret_cast<AST*>(malloc(sizeof(AST)));
			*array = {};
			array->type                  = ASTType::array;
			array->array.underlying_type = underlying_type;

			AST* capacity = reinterpret_cast<AST*>(malloc(sizeof(AST)));
			*capacity = {};
			capacity->type        = ASTType::tokens;
			capacity->tokens.node = 0;

			TokenNode** nil = &capacity->tokens.node;
			for (token = shift(tokenizer, 1); token.kind != static_cast<TokenKind>(']'); token = shift(tokenizer, 1))
			{
				ASSERT(token.kind != TokenKind::null);
				*nil = init_single_token_node(token);
				nil  = &(*nil)->next;
			}

			array->array.capacity            = capacity;
			ast->declaration.underlying_type = array;

			token = shift(tokenizer, 1);
		}

		return ast;
	}

	free(ast);
	return 0;
}

internal void deinit_ast(AST* ast)
{
	if (ast)
	{
		lambda deinit_entire_ast_node =
			[](ASTNode* node)
			{
				for (ASTNode* curr_node = node; curr_node;)
				{
					ASTNode* tail = curr_node->next;
					deinit_ast(curr_node->ast);
					free(curr_node);
					curr_node = tail;
				}
			};

		lambda deinit_entire_token_node =
			[](TokenNode* node)
			{
				for (TokenNode* curr_node = node; curr_node;)
				{
					TokenNode* tail = curr_node->next;
					free(curr_node);
					curr_node = tail;
				}
			};

		switch (ast->type)
		{
			case ASTType::root_type:
			{
			} break;

			case ASTType::array:
			{
				deinit_ast(ast->array.underlying_type);
				deinit_ast(ast->array.capacity);
			} break;

			case ASTType::container:
			{
				deinit_entire_ast_node(ast->container.declarations);
			} break;

			case ASTType::enumerator:
			{
				deinit_ast(ast->enumerator.underlying_type);
				deinit_entire_ast_node(ast->enumerator.declarations);
			} break;

			case ASTType::declaration:
			{
				deinit_ast(ast->declaration.underlying_type);
				deinit_ast(ast->declaration.assignment);
			} break;

			case ASTType::tokens:
			{
				deinit_entire_token_node(ast->tokens.node);
			} break;

			default:
			{
				ASSERT(!"Internal error :: Unexpected AST type");
				exit(1);
			} break;
		}

		free(ast);
	}
}

int main()
{
	DEFER { ASSERT(DEBUG_ALLOCATION_COUNT == 0); };

	constexpr String INCLUDE_META_DIRECTIVE = STR_OF("include \"meta/");

	Tokenizer main_tokenizer = init_tokenizer(SRC_DIR "HandmadeRalph.cpp");
	DEFER { deinit_tokenizer(&main_tokenizer); };

	while (true)
	{
		Token main_token = shift(&main_tokenizer, 0);

		if (main_token.kind == TokenKind::null)
		{
			break;
		}
		else if (main_token.kind == TokenKind::preprocessor && starts_with(main_token.str, INCLUDE_META_DIRECTIVE))
		{
			String operation = trunc(shift(main_token.str, INCLUDE_META_DIRECTIVE.size), '/');
			String file_path = trunc(shift(main_token.str, '"'), '"');

			File file = init_file(file_path);
			DEFER { deinit_file(&file); };

			if (operation == STR_OF("asset"))
			{
				Tokenizer meta_tokenizer = main_tokenizer;
				Token     meta_token     = main_token;

				while (!(meta_token.kind == TokenKind::identifier && meta_token.str == STR_OF("union")))
				{
					meta_token = shift(&meta_tokenizer, -1);
					ASSERT(meta_token.kind != TokenKind::null);
				}

				AST* ast = init_ast(&meta_tokenizer);
				DEFER { deinit_ast(ast); };

				ASSERT(write(&file, STR_OF("global inline constexpr wstrlit META_")));
				ASSERT(write(&file, trunc(rtrunc(file_path, '/'), '.')));
				ASSERT(write(&file, STR_OF("[] =\n\t{\n")));

				ASSERT(ast->type == ASTType::container);
				ASSERT(ast->container.type == ContainerType::a_union);
				ASSERT(ast->container.declarations->ast->declaration.underlying_type->type == ASTType::container);

				StringBuilder post_asserts = {};

				FOR_NODES(declaration, ast->container.declarations->ast->declaration.underlying_type->container.declarations)
				{
					ASSERT(+declaration->ast->meta);

					write(&file, STR_OF("\t\t"));
					String remaining = declaration->ast->meta;
					i32    count     = 0;
					while (true)
					{
						write(&file, STR_OF("DATA_DIR L\""));
						write(&file, trim_whitespace(trunc(remaining, ',')));
						write(&file, STR_OF("\""));

						remaining  = shift(remaining, ',');
						count     += 1;
						if (+remaining)
						{
							write(&file, STR_OF(", "));
						}
						else
						{
							break;
						}
					}
					if (declaration->next)
					{
						write(&file, STR_OF(","));
					}
					write(&file, STR_OF("\n"));

					// @TODO@ Check?
					append(&post_asserts, "static_assert(");
					if (declaration->ast->declaration.underlying_type->type == ASTType::array)
					{
						FOR_NODES(token, declaration->ast->declaration.underlying_type->array.capacity->tokens.node)
						{
							append(&post_asserts, "%.*s", PASS_STR(token->token.str));
						}
					}
					else
					{
						append(&post_asserts, "1");
					}

					append(&post_asserts, " == %d);\n", count);
				}

				ASSERT(write(&file, STR_OF("\t};\n")));
				ASSERT(write(&file, build(&post_asserts)));
			}
			else if (operation == STR_OF("enum"))
			{
				Tokenizer meta_tokenizer = main_tokenizer;
				Token     meta_token     = main_token;

				while (!(meta_token.kind == TokenKind::identifier && meta_token.str == STR_OF("enum")))
				{
					meta_token = shift(&meta_tokenizer, -1);
					ASSERT(meta_token.kind != TokenKind::null);
				}

				AST* ast = init_ast(&meta_tokenizer);
				DEFER { deinit_ast(ast); };

				ASSERT(ast->type == ASTType::enumerator);
				ASSERT(+ast->meta);
				ASSERT(ast->enumerator.underlying_type->type == ASTType::root_type);

				write(&file, STR_OF("global inline constexpr struct META_"));
				write(&file, ast->enumerator.name);
				write(&file, STR_OF("_t { "));
				write(&file, ast->enumerator.name);
				write(&file, STR_OF(" enumerator; "));
				write(&file, ast->enumerator.underlying_type->root_type.name);
				write(&file, STR_OF(" value; union { String str; struct { char PADDING_[offsetof(String, data)]; strlit cstr; }; }; "));
				write(&file, ast->meta);
				write(&file, STR_OF(" } META_"));
				write(&file, ast->enumerator.name);
				write(&file, STR_OF("[] =\n\t{\n"));

				FOR_NODES(declaration, ast->enumerator.declarations)
				{
					write(&file, STR_OF("\t\t{ "));

					write(&file, declaration->ast->declaration.name);
					write(&file, STR_OF(", static_cast<"));
					write(&file, ast->enumerator.underlying_type->root_type.name);
					write(&file, STR_OF(">("));
					write(&file, declaration->ast->declaration.name);
					write(&file, STR_OF("), STR_OF(\""));
					write(&file, declaration->ast->declaration.name);
					write(&file, STR_OF("\"), "));

					ASSERT(+declaration->ast->meta);
					write(&file, declaration->ast->meta);

					write(&file, STR_OF(" }"));
					if (declaration->next)
					{
						write(&file, STR_OF(","));
					}
					write(&file, STR_OF("\n"));
				}

				write(&file, STR_OF("\t};"));
			}
			else
			{
				// @TODO@ Line number and locality.
				printf(":: Unknown include meta directive : `%.*s`\n", PASS_STR(main_token.str));
				return 1;
			}
		}

		shift(&main_tokenizer, 1);
	}

	return 0;
}
