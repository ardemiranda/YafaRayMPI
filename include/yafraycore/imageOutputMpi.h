/****************************************************************************
 *
 * 		imageOutput.h: generic color output based on imageHandlers
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

#ifndef Y_IMAGE_OUTPUT_MPI_H
#define Y_IMAGE_OUTPUT_MPI_H

#include <core_api/imagehandler.h>
#include <core_api/output.h>
#include <mpi.h>

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT imageOutputMpi_t : public colorOutput_t
{
	public:
		imageOutputMpi_t(imageHandler_t *handle, const std::string &name, int bx, int by);
		imageOutputMpi_t(); //!< Dummy initializer
		virtual ~imageOutputMpi_t();
		void loadMpi();
		virtual bool putPixel(int x = 0, int y = 0, const float *c = NULL, bool alpha = true, bool depth = false, float z = 0.f);
		virtual void flush();
		virtual void flushArea(int x0, int y0, int x1, int y1) {}; // not used by images... yet
	protected:
		int tamanhoBuffer;
		int positionBuffer;
		char *buffer;
		int rank;
		MPI::Prequest request;
	private:
		imageHandler_t *image;
		std::string fname;
		float bX;
		float bY;
};

__END_YAFRAY

#endif // Y_IMAGE_OUTPUT_H

