#include <stdio.h>
#include <stdlib.h>
#include "unified.h"
#undef ASSERT
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
#define PASS_STRING(STRING) (STRING).size, (STRING).data
#define META_TAG "@META@"

enum struct TokenKind : u8
{
	nul,

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
	TokenKind  kind;
	StringView string;
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
	TokenBufferNode* curr_token_buffer_node;
	i32              file_size; // @NOTICE@ Only handles files below 2.14GB. If each line of code averages at 256 characters, this would be a concern if there were about 8,388,608 LOC in processed file.
	char*            file_data;
};

internal Tokenizer init_tokenizer(strlit file_path)
{
	Tokenizer tokenizer = {};

	FILE* file;
	ASSERT(fopen_s(&file, file_path, "rb") != EINVAL); // @TODO@ Handle error.
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
					{ // @NOTICE@ Doesn't handle escaped newlines, but why would anyone do that?
						curr_read += 2;

						while (curr_read < read_eof && curr_read[0] == ' ')
						{
							curr_read += 1;
						}

						if (curr_read + sizeof(META_TAG) < read_eof && memcmp(curr_read, META_TAG, sizeof(META_TAG) - 1) == 0)
						{
							curr_read += sizeof(META_TAG) - 1;

							while (curr_read < read_eof && curr_read[0] == ' ')
							{
								curr_read += 1;
							}

							token.kind        = TokenKind::meta;
							token.string.data = curr_read;

							while (curr_read + token.string.size < read_eof && curr_read[token.string.size] != '\r' && curr_read[token.string.size] != '\n')
							{
								token.string.size += 1;
							}
							curr_read += token.string.size;
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
						token.string.data = curr_read;

						if (curr_read[0] == '#' && can_be_preprocessor)
						{
							token.kind = TokenKind::preprocessor;

							bool32 escaped = false;
							while (curr_read + token.string.size < read_eof && IMPLIES(curr_read[token.string.size] == '\r' || curr_read[token.string.size] == '\n', escaped))
							{
								if (curr_read[token.string.size] == '\\')
								{
									escaped = !escaped;
								}
								else if (curr_read[token.string.size] != '\r')
								{
									escaped = false;
								}

								token.string.size += 1;
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
									curr_read + token.string.size < read_eof &&
									(
										curr_read[token.string.size] == '_'
										|| is_alpha(curr_read[token.string.size])
										|| is_digit(curr_read[token.string.size])
									)
								)
								{
									token.string.size += 1;
								}
							}
							else if (is_digit(curr_read[0]) || curr_read[0] == '.' && curr_read + 1 < read_eof && is_digit(curr_read[1]))
							{
								token.kind = TokenKind::number;

								while
								(
									curr_read + token.string.size < read_eof &&
									(
										is_digit(curr_read[token.string.size])
										|| is_alpha(curr_read[token.string.size]) // @TODO@ Suffixes.
										|| curr_read[token.string.size] == '.'
										|| curr_read[token.string.size] == '\''
									)
								)
								{
									token.string.size += 1;
								}
							}
							else if (curr_read[0] == '"')
							{
								token.kind         = TokenKind::string;
								token.string.data += 1;
								curr_read         += 1;

								bool32 escaped = false;
								while (curr_read + token.string.size < read_eof && IMPLIES(curr_read[token.string.size] == '"', escaped))
								{
									if (curr_read[token.string.size] == '\\')
									{
										escaped = !escaped;
									}
									else
									{
										escaped = false;
									}

									token.string.size += 1;
								}
								curr_read += 1;
							}
							else if (curr_read[0] == '\'')
							{
								token.kind         = TokenKind::character;
								token.string.size  = 1;
								token.string.data += 1;
								curr_read         += 1;

								if (curr_read < read_eof && curr_read[0] == '\\')
								{
									token.string.data += 1;
									curr_read         += 1;
								}

								curr_read += 1;
							}
							else
							{
								token.kind         = static_cast<TokenKind>(curr_read[0]),
								token.string.size += 1;
							}
						}
						curr_read += token.string.size; // @TODO@ Distribute this into all the cases.
					}
				} break;
			}

			if (token.kind != TokenKind::nul)
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
	while (node && node->prev)
	{
		node = node->prev;
	}
	while (node && node->next)
	{
		TokenBufferNode* tail = node->next;
		free(node);
		node = tail;
	}
}

int main()
{
	Tokenizer tokenizer = init_tokenizer(SRC_DIR "HandmadeRalph.cpp");
	DEFER { deinit_tokenizer(&tokenizer); };

	#if 0
	FOR_NODES(node, tokenizer.curr_token_buffer_node)
	{
		FOR_ELEMS(token, node->buffer, node->count)
		{
			printf("%d : `%.*s`\n", static_cast<i32>(token->kind), PASS_STRING(token->string));
		}
	}
	#else
	FOR_ELEMS(token, tokenizer.curr_token_buffer_node->buffer, 256)
	{
		printf("%d : \"%.*s\"\n", static_cast<i32>(token->kind), PASS_STRING(token->string));
	}
	#endif

	DEBUG_STDOUT_HALT();
	return 0;
}
