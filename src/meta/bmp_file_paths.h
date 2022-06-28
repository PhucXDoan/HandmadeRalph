static inline constexpr wstrlit BMP_FILE_PATHS[] =
	{
		DATA_DIR L"test_hero_left_head.bmp" , DATA_DIR L"test_hero_right_head.bmp" , DATA_DIR L"test_hero_front_head.bmp" , DATA_DIR L"test_hero_back_head.bmp",
		DATA_DIR L"test_hero_left_cape.bmp" , DATA_DIR L"test_hero_right_cape.bmp" , DATA_DIR L"test_hero_front_cape.bmp" , DATA_DIR L"test_hero_back_cape.bmp",
		DATA_DIR L"test_hero_left_torso.bmp", DATA_DIR L"test_hero_right_torso.bmp", DATA_DIR L"test_hero_front_torso.bmp", DATA_DIR L"test_hero_back_torso.bmp",
		DATA_DIR L"test_hero_shadow.bmp",
		DATA_DIR L"test_background.bmp"
	};
static_assert(4 == 4);
static_assert(4 == 4);
static_assert(4 == 4);
static_assert(1 == 1);
static_assert(1 == 1);
static_assert(ARRAY_CAPACITY(bmps) == ARRAY_CAPACITY(BMP_FILE_PATHS));
