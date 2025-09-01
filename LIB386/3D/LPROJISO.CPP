#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <3D/SINTAB.H>
#include <3D/CAMERA.H>
#include <stdio.h>

S32 LongProjectPointIso(S32 x, S32 y, S32 z)
{
	x -= CameraX;
	y -= CameraY;
	z -= CameraZ;
	int tmpx = x;
	x = x - z;
	z = z + tmpx;

	int tmpy = y;
	z = z * 2;
	y = y << 4; // *16
	x = x * 3;

	y = y - tmpy;
	z = z * 3;

	x = x >> 6; // /64 IsoScale
	x = x + XCentre;
	z = z - y;

	z = z >> 8; // /256 IsoScale
	z = z + YCentre + 1;

	Xp = x;
	Yp = z;

	return -1;
}
