












void fill_joint_coords(struct arena *arena, struct joint *joint)
{
	float x = joint->x;
	float y = joint->y;
	float a0, x0, y0;
	float a1, x1, y1;
	float r = 4.0f;
	float *coords = arena->joint_coords;
	int i;

	for (i = 0; i < 8; i++) {
		a0 = i * (TAU / 8);
		a1 = (i + 1) * (TAU / 8);
		x0 = x + cosf(a0) * r;
		y0 = y + sinf(a0) * r;
		x1 = x + cosf(a1) * r;
		y1 = y + sinf(a1) * r;
		world_to_view(&arena->view, x,  y,  coords + 2, coords + 3);
		world_to_view(&arena->view, x0, y0, coords + 0, coords + 1);
		world_to_view(&arena->view, x1, y1, coords + 4, coords + 5);
		coords += 6;
	}
}