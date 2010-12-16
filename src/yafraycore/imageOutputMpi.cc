
/****************************************************************************
 *
 * 		imageOutput.cc: generic color output based on imageHandlers
 *      This is part of the yafray package
 *      Copyright (C) 2010 Rodrigo Placencia Vazquez
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2.1 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library; if not, write to the Free Software
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *      
 */

#include <yafraycore/imageOutputMpi.h>

__BEGIN_YAFRAY

imageOutputMpi_t::imageOutputMpi_t(imageHandler_t * handle, const std::string &name, int bx, int by) : image(handle), fname(name), bX(bx), bY(by)
{
	//empty
	loadMpi();
}

imageOutputMpi_t::imageOutputMpi_t()
{
	image = NULL;
	loadMpi();
}

imageOutputMpi_t::~imageOutputMpi_t()
{
	request.Wait();
	request.Free();
	image = NULL;
}

void imageOutputMpi_t::loadMpi() {
	tamanhoBuffer = sizeof(int)*2 + sizeof(const float[4]) + sizeof(bool) + sizeof(float);
	buffer = (char *) malloc(tamanhoBuffer);

	rank = MPI::COMM_WORLD.Get_rank();
	if (rank == 0) {
		request = MPI::COMM_WORLD.Recv_init(buffer, tamanhoBuffer, MPI::PACKED, MPI::ANY_SOURCE, 1);
	} else {
		request = MPI::COMM_WORLD.Send_init(buffer, tamanhoBuffer, MPI::PACKED, 0, 1);
	}
}

bool imageOutputMpi_t::putPixel(int x, int y, const float *c, bool alpha, bool depth, float z)
{
	positionBuffer = 0;

	if (rank == 0) {
		float mpiC[4] = {0,0,0,0};
		request.Start();
		request.Wait();

		MPI::INT.Unpack(buffer, tamanhoBuffer, &x, 1, positionBuffer, MPI::COMM_WORLD);
		MPI::INT.Unpack(buffer, tamanhoBuffer, &y, 1, positionBuffer, MPI::COMM_WORLD);
		MPI::BYTE.Unpack(buffer, tamanhoBuffer, mpiC, sizeof(const float[4]), positionBuffer, MPI::COMM_WORLD);
	    MPI::BOOL.Unpack(buffer, tamanhoBuffer, &alpha, 1, positionBuffer, MPI::COMM_WORLD);
	    MPI::FLOAT.Unpack(buffer, tamanhoBuffer, &z, 1, positionBuffer, MPI::COMM_WORLD);

		if(image)
		{
			colorA_t col(0.f);
			col.set(mpiC[0], mpiC[1], mpiC[2], ( (alpha) ? mpiC[3] : 1.f ) );
			image->putPixel(x + bX , y + bY, col, z);
		}

	} else {
		MPI::INT.Pack(&x, 1, buffer, tamanhoBuffer, positionBuffer, MPI::COMM_WORLD);
		MPI::INT.Pack(&y, 1, buffer, tamanhoBuffer, positionBuffer, MPI::COMM_WORLD);
		MPI::BYTE.Pack((char *) c, sizeof(const float[4]), buffer, tamanhoBuffer, positionBuffer, MPI::COMM_WORLD);
		MPI::BOOL.Pack(&alpha, 1, buffer, tamanhoBuffer, positionBuffer, MPI::COMM_WORLD);
		MPI::FLOAT.Pack(&z, 1, buffer, tamanhoBuffer, positionBuffer, MPI::COMM_WORLD);

		request.Start();
		request.Wait();
	}


	return true;
}

void imageOutputMpi_t::flush()
{
	if (rank == 0 && image) {
		image->saveToFile(fname);
	}
}

__END_YAFRAY

