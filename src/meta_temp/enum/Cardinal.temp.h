global inline constexpr struct META_Cardinal_t { Cardinal enumerator; u8 value; union { String str; struct { char PADDING_[4]; strlit cstr; }; }; vf2 vf; vi2 vi; } META_Cardinal[] =
	{
		{ Cardinal_left,  static_cast<u8>(Cardinal_left),  STR_OF("left" ), { -1.0f,  0.0f }, { -1,  0 } },
		{ Cardinal_right, static_cast<u8>(Cardinal_right), STR_OF("right"), {  1.0f,  0.0f }, {  1,  0 } },
		{ Cardinal_down,  static_cast<u8>(Cardinal_down),  STR_OF("down" ), {  0.0f, -1.0f }, {  0, -1 } },
		{ Cardinal_up,    static_cast<u8>(Cardinal_up),    STR_OF("up"   ), {  0.0f,  1.0f }, {  0,  1 } }
	};
