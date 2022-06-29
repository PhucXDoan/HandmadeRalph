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
#define PASS_STR(STRING)  (STRING).size, (STRING).data
#define META_TAG "@META@"

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
	i32              curr_token_index;
	TokenBufferNode* curr_token_buffer_node;
	i32              file_size; // @NOTICE@ Only handles files below 2.14GB. If each line of code averages at 256 characters, this would be a concern if there were about 8,388,608 LOC in processed file.
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
							token.kind          = TokenKind::preprocessor;
							token.string.data += 1;
							curr_read         += 1;

							bool32 escaped = false;
							while
							(
								curr_read + token.string.size < read_eof
								&& IMPLIES(curr_read[token.string.size] == '\r' || curr_read[token.string.size] == '\n', escaped)
								&& IMPLIES(curr_read + token.string.size + 1 < read_eof, !(curr_read[token.string.size] == '/' && curr_read[token.string.size + 1] == '/'))
								&& IMPLIES(curr_read + token.string.size + 1 < read_eof, !(curr_read[token.string.size] == '/' && curr_read[token.string.size + 1] == '*'))
							)
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

internal Token shift_tokenizer(Tokenizer* tokenizer, i32 offset)
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

int main()
{
	Tokenizer main_tokenizer = init_tokenizer(SRC_DIR "HandmadeRalph.cpp");
	DEFER { deinit_tokenizer(&main_tokenizer); };

	while (true)
	{
		Token curr_main_token = shift_tokenizer(&main_tokenizer, 0);

		if (curr_main_token.kind == TokenKind::null)
		{
			break;
		}
		else if (curr_main_token.kind == TokenKind::meta)
		{ // @TODO@ This only handles a specific usage of the metaprogram.
			Tokenizer asset_tokenizer = main_tokenizer;
			Token     asset_token;

			StringView asset_array_name;
			while (true)
			{
				asset_token = shift_tokenizer(&asset_tokenizer, 1);
				ASSERT(asset_token.kind != TokenKind::null);

				if (asset_token.kind == static_cast<TokenKind>('}'))
				{
					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == TokenKind::identifier); // @NOTE@ Expects a name for the containing struct.

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == static_cast<TokenKind>(';'));

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == TokenKind::identifier); // @NOTE@ Expects the underlying type of the asset.

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == TokenKind::identifier); // @NOTE@ Expects the name of the asset array.

					asset_array_name = asset_token.string;
					break;
				}
			}

			do
			{
				asset_token = shift_tokenizer(&asset_tokenizer, 1);
				ASSERT(asset_token.kind != TokenKind::null);
			}
			while (asset_token.kind != static_cast<TokenKind>('}'));

			asset_token = shift_tokenizer(&asset_tokenizer, 1);
			ASSERT(asset_token.kind == static_cast<TokenKind>(';'));

			constexpr StringView PREPROCESSOR_INCLUDE_PREFIX = STRING_VIEW_OF("include ");
			Token preprocessor_include = shift_tokenizer(&asset_tokenizer, 1);
			ASSERT(preprocessor_include.kind == TokenKind::preprocessor);
			ASSERT(starts_with(preprocessor_include.string, PREPROCESSOR_INCLUDE_PREFIX));

			StringView output_asset_file_path = { .size = 3, .data = preprocessor_include.string.data + PREPROCESSOR_INCLUDE_PREFIX.size + 1 };
			while
			(
				output_asset_file_path.data + output_asset_file_path.size < preprocessor_include.string.data + preprocessor_include.string.size
				&& output_asset_file_path.data[output_asset_file_path.size] != '"'
				&& output_asset_file_path.data[output_asset_file_path.size] != '>'
			)
			{
				output_asset_file_path.size += 1;
			}

			ASSERT(starts_with(output_asset_file_path, STRING_VIEW_OF("meta/")));

			main_tokenizer = asset_tokenizer;

			FILE* output_asset_file = 0;
			{
				char buffer[256];
				i32 result = sprintf_s(buffer, SRC_DIR "%.*s", PASS_STR(output_asset_file_path));
				ASSERT(result == static_cast<i32>(sizeof(SRC_DIR) - 1 + output_asset_file_path.size));

				result = fopen_s(&output_asset_file, buffer, "wb");
				ASSERT(result != EINVAL);
				ASSERT(output_asset_file);
			}
			DEFER { fclose(output_asset_file); };

			fprintf(output_asset_file, "static inline constexpr wstrlit ");
			{
				char   buffer[256];
				i32    count   = 0;
				bool32 writing = false;
				FOR_ELEMS(c, output_asset_file_path.data, output_asset_file_path.size)
				{
					if (writing)
					{
						if (*c == '.')
						{
							break;
						}
						else
						{
							buffer[count]  = uppercase(*c);
							count         += 1;
						}
					}
					else if (*c == '/')
					{
						writing = true;
					}
				}
				ASSERT(writing);
				fprintf(output_asset_file, "%.*s", count, buffer);
			}
			fprintf(output_asset_file, "[] =\n\t{\n");

			do
			{
				asset_token = shift_tokenizer(&asset_tokenizer, -1);
				ASSERT(asset_token.kind != TokenKind::null);
			}
			while (!(asset_token.kind == TokenKind::identifier && asset_token.string == STRING_VIEW_OF("struct")));

			asset_token = shift_tokenizer(&asset_tokenizer, 1);
			ASSERT(asset_token.kind == static_cast<TokenKind>('{'));

			while (true)
			{
				asset_token = shift_tokenizer(&asset_tokenizer, 1);
				ASSERT(asset_token.kind != TokenKind::null);

				if (asset_token.kind == static_cast<TokenKind>('}'))
				{
					break;
				}
				else
				{
					ASSERT(asset_token.kind == TokenKind::identifier); // @NOTE@ Expects the type of the asset.

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == TokenKind::identifier); // @NOTE@ Expects the name of the asset.

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind != TokenKind::null || asset_token.kind == static_cast<TokenKind>(';') || asset_token.kind == static_cast<TokenKind>('['));

					i32 asset_count = 1;
					if (asset_token.kind == static_cast<TokenKind>('['))
					{
						asset_token = shift_tokenizer(&asset_tokenizer, 1);
						ASSERT(asset_token.kind == TokenKind::number); // @TODO@ Other expressions could be the size.

						{
							i32 result = _snscanf_s(asset_token.string.data, static_cast<size_t>(asset_token.string.size), "%d", &asset_count);
							ASSERT(result == 1);
						}

						asset_token = shift_tokenizer(&asset_tokenizer, 1);
						ASSERT(asset_token.kind == static_cast<TokenKind>(']'));

						asset_token = shift_tokenizer(&asset_tokenizer, 1);
						ASSERT(asset_token.kind == static_cast<TokenKind>(';'));
					}

					asset_token = shift_tokenizer(&asset_tokenizer, 1);
					ASSERT(asset_token.kind == TokenKind::meta);

					fprintf(output_asset_file, "\t\t");

					i32 asset_file_path_start = 0;
					while (true)
					{
						while (asset_file_path_start < asset_token.string.size && asset_token.string.data[asset_file_path_start] == ' ')
						{
							asset_file_path_start += 1;
						}

						if (asset_file_path_start < asset_token.string.size)
						{

							i32 asset_file_path_length = 0;
							while (asset_file_path_start + asset_file_path_length < asset_token.string.size && asset_token.string.data[asset_file_path_start + asset_file_path_length] != ',')
							{
								asset_file_path_length += 1;
							}

							while (asset_file_path_length > 0 && asset_token.string.data[asset_file_path_start + asset_file_path_length - 1] == ' ')
							{
								asset_file_path_length -= 1;
							}

							ASSERT(asset_file_path_length >= 1);

							fprintf(output_asset_file, "DATA_DIR L\"%.*s\",", asset_file_path_length, asset_token.string.data + asset_file_path_start);

							asset_count -= 1;
							if (asset_count)
							{
								fprintf(output_asset_file, " ");
							}

							asset_file_path_start += asset_file_path_length;
							while (asset_file_path_start < asset_token.string.size && asset_token.string.data[asset_file_path_start] != ',')
							{
								asset_file_path_start += 1;
							}
							asset_file_path_start += 1;
						}
						else
						{
							break;
						}
					}
					fprintf(output_asset_file, "\n");
				}
			}

			fprintf(output_asset_file, "\t};\n");
		}

		shift_tokenizer(&main_tokenizer, 1);
	}

	return 0;
}
