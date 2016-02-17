/*
* affine.h -- Affine Transforms for 2d objects
* Copyright (C) 2002 Charles Yates <charles.yates@pandora.be>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _KINOPLUS_AFFINE_H
#define _KINOPLUS_AFFINE_H

#include <cmath>

/** Affine transforms for 2d image manipulation. Current provides shearing and
	rotating support.
*/

class AffineTransform
{
protected:
	double matrix[ 2 ][ 2 ];

	// Multiply two this affine transform with that
	void Multiply( AffineTransform that )
	{
		double output[ 2 ][ 2 ];

		for ( int i = 0; i < 2; i ++ )
			for ( int j = 0; j < 2; j ++ )
				output[ i ][ j ] = matrix[ i ][ 0 ] * that.matrix[ j ][ 0 ] +
				                   matrix[ i ][ 1 ] * that.matrix[ j ][ 1 ];

		matrix[ 0 ][ 0 ] = output[ 0 ][ 0 ];
		matrix[ 0 ][ 1 ] = output[ 0 ][ 1 ];
		matrix[ 1 ][ 0 ] = output[ 1 ][ 0 ];
		matrix[ 1 ][ 1 ] = output[ 1 ][ 1 ];
	}

public:
	// Define a default matrix
	AffineTransform()
	{
		matrix[ 0 ][ 0 ] = 1;
		matrix[ 0 ][ 1 ] = 0;
		matrix[ 1 ][ 0 ] = 0;
		matrix[ 1 ][ 1 ] = 1;
	}

	// Rotate by a given angle
	void Rotate( double angle )
	{
		AffineTransform affine;
		affine.matrix[ 0 ][ 0 ] = cos( angle * M_PI / 180 );
		affine.matrix[ 0 ][ 1 ] = 0 - sin( angle * M_PI / 180 );
		affine.matrix[ 1 ][ 0 ] = sin( angle * M_PI / 180 );
		affine.matrix[ 1 ][ 1 ] = cos( angle * M_PI / 180 );
		Multiply( affine );
	}

	// Shear by a given value
	void Shear( double shear )
	{
		AffineTransform affine;
		affine.matrix[ 0 ][ 0 ] = 1;
		affine.matrix[ 0 ][ 1 ] = shear;
		affine.matrix[ 1 ][ 0 ] = 0;
		affine.matrix[ 1 ][ 1 ] = 1;
		Multiply( affine );
	}

	void Scale( double sx, double sy )
	{
		AffineTransform affine;
		affine.matrix[ 0 ][ 0 ] = sx;
		affine.matrix[ 0 ][ 1 ] = 0;
		affine.matrix[ 1 ][ 0 ] = 0;
		affine.matrix[ 1 ][ 1 ] = sy;
		Multiply( affine );
	}

	// Obtain the mapped x coordinate of the input
	double MapX( int x, int y )
	{
		return matrix[ 0 ][ 0 ] * x + matrix[ 0 ][ 1 ] * y;
	}

	// Obtain the mapped y coordinate of the input
	double MapY( int x, int y )
	{
		return matrix[ 1 ][ 0 ] * x + matrix[ 1 ][ 1 ] * y;
	}
};

#endif

