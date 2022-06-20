global f32 g_hertz = 512.0f;

PlatformUpdate_t(PlatformUpdate)
{
	DEBUG_persist vi2 offset;

	if (BTN_DOWN(.gamepads[0].action_left))
	{
		offset.x -= 1;
	}
	if (BTN_DOWN(.gamepads[0].action_right))
	{
		offset.x += 1;
	}
	if (BTN_DOWN(.gamepads[0].action_down))
	{
		offset.y += 1;
	}
	if (BTN_DOWN(.gamepads[0].action_up))
	{
		offset.y -= 1;
	}

	FOR_RANGE(y, platform_framebuffer->dimensions.y)
	{
		FOR_RANGE(x, platform_framebuffer->dimensions.x)
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dimensions.x + x] = vxx_argb(vi3 { offset.x + x, offset.y + y, offset.x + x + offset.y + y });
		}
	}
}

PlatformSound_t(PlatformSound)
{
	//DEBUG_persist f32 t;
	//t += 1.0f / (platform_samples_per_second / g_hertz) * TAU;
	//return { static_cast<i16>(sinf(t) * 500.0f), static_cast<i16>(sinf(t) * 500.0f) };

	return {};
}
