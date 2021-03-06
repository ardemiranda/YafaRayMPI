%module yafrayinterface

%include "cpointer.i"
%pointer_functions(float, floatp);
%pointer_functions(int, intp);
%pointer_functions(unsigned int, uintp);

%include "carrays.i"
%include "std_string.i"
%include "std_vector.i"

%array_functions(float, floatArray);

namespace std
{
	%template(StrVector) vector<string>;
}

%{
#include <yafraycore/monitor.h>
#include <core_api/output.h>
#include <interface/yafrayinterface.h>

struct yafTilePixel_t
{
	float r;
	float g;
	float b;
	float a;
};

struct YafTileObject_t
{
	PyObject_HEAD
	int resx, resy;
	int x0, x1, y0, y1;
	int w, h;
	yafTilePixel_t *mem;
};

static Py_ssize_t yaf_tile_length(YafTileObject_t *self)
{
	self->w = (self->x1 - self->x0);
	self->h = (self->y1 - self->y0);
	
	return self->w * self->h;
}

static PyObject *yaf_tile_subscript_int(YafTileObject_t *self, int keynum)
{
	// Check boundaries and fill w and h
	if (keynum >= yaf_tile_length(self))
		return NULL;
	
	// Calc position on tile
	int vy = keynum / self->w;
	int vx = keynum - vy * self->w;
	
	// Map tile position to image buffer
	vx = self->x0 + vx;
	vy = (self->y0 + self->h - 1) - vy;
	
	// Get pixel
	yafTilePixel_t &pix = self->mem[ self->resx * vy + vx ];

	return Py_BuildValue("ffff", 
		pix.r,
		pix.g,
		pix.b,
		pix.a);
}

static void yaf_tile_dealloc(YafTileObject_t *self)
{
	PyObject_Del(self);
}

PySequenceMethods sequence_methods =
{
	( lenfunc ) yaf_tile_length,
	NULL,
	NULL,
	( ssizeargfunc ) yaf_tile_subscript_int
};

PyTypeObject yafTile_Type =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"yaf_tile",							/* tp_name */
	sizeof(YafTileObject_t),			/* tp_basicsize */
	0,									/* tp_itemsize */
	( destructor ) yaf_tile_dealloc,	/* tp_dealloc */
	NULL,                       		/* printfunc tp_print; */
	NULL,								/* getattrfunc tp_getattr; */
	NULL,								/* setattrfunc tp_setattr; */
	NULL,								/* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,								/* tp_repr */
	NULL,                       		/* PyNumberMethods *tp_as_number; */
	&sequence_methods,					/* PySequenceMethods *tp_as_sequence; */
	NULL,								/* PyMappingMethods *tp_as_mapping; */
	NULL,								/* hashfunc tp_hash; */
	NULL,								/* ternaryfunc tp_call; */
	NULL,                       		/* reprfunc tp_str; */
	NULL,								/* getattrofunc tp_getattro; */
	NULL,								/* setattrofunc tp_setattro; */
	NULL,                       		/* PyBufferProcs *tp_as_buffer; */
	Py_TPFLAGS_DEFAULT,         		/* long tp_flags; */
};


class pyOutput_t : public yafaray::colorOutput_t
{

public:

	pyOutput_t(int x, int y, PyObject *callback) : resx(x), resy(y), callb(callback)
	{
		tile = PyObject_NEW(YafTileObject_t, &yafTile_Type);
		tile->mem = new yafTilePixel_t[x*y];
		tile->resx = x;
		tile->resy = y;
	}
	
	virtual ~pyOutput_t()
	{
		delete [] tile->mem;
		Py_DECREF(tile);
	}
	
	virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f)
	{
		yafTilePixel_t &pix= tile->mem[resx * y + x]; 
		pix.r = c[0];
		pix.g = c[1];
		pix.b = c[2];
		pix.a = alpha ? c[3] : 1.0f;
		return true;
	}
	
	virtual void flush()
	{
		tile->x0 = 0;
		tile->x1 = resx;
		tile->y0 = 0;
		tile->y1 = resy;
		
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		PyEval_CallObject(callb, Py_BuildValue("siiO", "flush", resx, resy, tile));
		PyGILState_Release(gstate);
	}

	virtual void flushArea(int x0, int y0, int x1, int y1)
	{
		tile->x0 = x0;
		tile->x1 = x1;
		tile->y0 = y0;
		tile->y1 = y1;
		
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		PyEval_CallObject(callb, Py_BuildValue("siiiiO", "flushArea", x0, resy - y1, x1, resy - y0, tile));
		PyGILState_Release(gstate);
	}

	virtual void highliteArea(int x0, int y0, int x1, int y1)
	{
		tile->x0 = x0;
		tile->x1 = x1;
		tile->y0 = y0;
		tile->y1 = y1;
		
		int w = x1 - x0;
		int h = y1 - y0;
		int lineL = std::min( 4, std::min( h - 1, w - 1 ) );
		
		drawCorner(x0, y0, lineL, TL_CORNER);
		drawCorner(x1, y0, lineL, TR_CORNER);
		drawCorner(x0, y1, lineL, BL_CORNER);
		drawCorner(x1, y1, lineL, BR_CORNER);
		
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		PyEval_CallObject(callb, Py_BuildValue("siiiiO", "highliteArea", x0, resy - y1, x1, resy - y0, tile));
		PyGILState_Release(gstate);

	}
	
private:
	
	enum corner
	{
		TL_CORNER,
		TR_CORNER,
		BL_CORNER,
		BR_CORNER
	};
	
	void drawCorner(int x, int y, int len, corner pos)
	{
		int minX = 0;
		int minY = 0;
		int maxX = 0;
		int maxY = 0;
		
		switch(pos)
		{
			case TL_CORNER:
				minX = x;
				minY = y;
				maxX = x + len;
				maxY = y + len;
				break;
			
			case TR_CORNER:
				minX = x - len - 1;
				minY = y;
				maxX = x - 1;
				maxY = y + len;
				x--;
				break;
			
			case BL_CORNER:
				minX = x;
				minY = y - len - 1;
				maxX = x + len;
				maxY = y - 1;
				y--;
				break;
			
			case BR_CORNER:
				minX = x - len - 1;
				minY = y - len - 1;
				maxX = x - 1;
				maxY = y - 1;
				x--;
				y--;
				break;
		}

		for(int i = minX; i < maxX; i++)
		{
			tile->mem[resx * y + i].r = 0.625f;
		}
		
		for(int j = minY; j < maxY; j++)
		{
			tile->mem[resx * j + x].r = 0.625f;
		}
	}

	int resx, resy;
	PyObject *callb;
	YafTileObject_t *tile;
};

class pyProgress : public yafaray::progressBar_t
{

public:

	pyProgress(PyObject *callback) : callb(callback) {}
	
	void report_progress(float percent)
	{
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		PyEval_CallObject(callb, Py_BuildValue("sf", "progress", percent));
		PyGILState_Release(gstate);
	}
	
	virtual void init(int totalSteps)
	{
		steps_to_percent = 100.f / (float) totalSteps;
		doneSteps = 0;
		report_progress(0.f);
	}
	
	virtual void update(int steps = 1)
	{
		doneSteps += steps;
		report_progress(doneSteps * steps_to_percent);		
	}
	
	virtual void done()
	{
		report_progress(100.f);
	}
	
	virtual void setTag(const char* text)
	{
		PyGILState_STATE gstate;
		gstate = PyGILState_Ensure();
		PyEval_CallObject(callb, Py_BuildValue("ss", "tag", text));
		PyGILState_Release(gstate);
	}
	
private:

	PyObject *callb;
	float steps_to_percent;
	int doneSteps;
};

%}

%init %{
	PyType_Ready(&yafTile_Type);
%}

%typemap(in) PyObject *pyfunc
{
  if (!PyCallable_Check($input))
  {
      PyErr_SetString(PyExc_TypeError, "Need a callback method.");
      return NULL;
  }
  
  $1 = $input;
}

%extend yafaray::yafrayInterface_t
{
	void render(int x, int y, PyObject *outputCallBack, PyObject *progressCallback)
	{
  		pyOutput_t output_wrap(x, y, outputCallBack);
  		pyProgress *pbar_wrap = new pyProgress(progressCallback);
  		
  		Py_BEGIN_ALLOW_THREADS;
  		self->render(output_wrap, pbar_wrap);
		Py_END_ALLOW_THREADS;
	}
}

%exception yafaray::yafrayInterface_t::loadPlugins
{
	Py_BEGIN_ALLOW_THREADS
	$action
	Py_END_ALLOW_THREADS
}

%{
#include <interface/yafrayinterface.h>
#include <interface/xmlinterface.h>
#include <yafraycore/imageOutput.h>
#include <yafraycore/memoryIO.h>
using namespace yafaray;
%}

namespace yafaray {

class colorOutput_t
{
	public:
		virtual ~colorOutput_t() {};
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f)=0;
		virtual void flush()=0;
		virtual void flushArea(int x0, int y0, int x1, int y1)=0;
};

class imageHandler_t
{
public:
	virtual void initForOutput(int width, int height, bool withAlpha = false, bool withDepth = true) = 0;
	virtual ~imageHandler_t() {};
	virtual bool loadFromFile(const std::string &name) = 0;
	virtual bool loadFromMemory(const yByte *data, size_t size) {return false; };
	virtual bool saveToFile(const std::string &name) = 0;
	virtual void putPixel(int x, int y, const colorA_t &rgba, float depth = 0.f) = 0;
	virtual colorA_t getPixel(int x, int y) = 0;
	
protected:
	std::string handlerName;
	int m_width;
	int m_height;
	bool m_hasAlpha;
	bool m_hasDepth;
	rgba2DImage_nw_t *m_rgba;
	gray2DImage_nw_t *m_depth;
};

class imageOutput_t : public colorOutput_t
{
	public:
		imageOutput_t(imageHandler_t *handle, const std::string &name, int bx, int by);
		imageOutput_t(); //!< Dummy initializer
		virtual ~imageOutput_t();
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f);
		virtual void flush();
		virtual void flushArea(int x0, int y0, int x1, int y1) {}; // not used by images... yet
	private:
		imageHandler_t *image;
		std::string fname;
};

class yafrayInterface_t
{
	public:
		yafrayInterface_t();
		virtual ~yafrayInterface_t();
		// directly related to scene_t:
		virtual void loadPlugins(const char *path); //!< load plugins from path, if NULL load from default path, if available.
		virtual bool startGeometry(); //!< call before creating geometry; only meshes and vmaps can be created in this state
		virtual bool endGeometry(); //!< call after creating geometry;
		/*! start a triangle mesh
			in this state only vertices, UVs and triangles can be created
			\param id returns the ID of the created mesh
		*/
		virtual unsigned int getNextFreeID();
		virtual bool startTriMesh(unsigned int id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool startCurveMesh(unsigned int id, int vertices);
		virtual bool startTriMeshPtr(unsigned int *id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool endTriMesh(); //!< end current mesh and return to geometry state
		virtual bool endCurveMesh(const material_t *mat, float strandStart, float strandEnd, float strandShape); //!< end current mesh and return to geometry state
		virtual int  addVertex(double x, double y, double z); //!< add vertex to mesh; returns index to be used for addTriangle
		virtual int  addVertex(double x, double y, double z, double ox, double oy, double oz); //!< add vertex with Orco to mesh; returns index to be used for addTriangle
		virtual bool addTriangle(int a, int b, int c, const material_t *mat); //!< add a triangle given vertex indices and material pointer
		virtual bool addTriangle(int a, int b, int c, int uv_a, int uv_b, int uv_c, const material_t *mat); //!< add a triangle given vertex and uv indices and material pointer
		virtual int  addUV(float u, float v); //!< add a UV coordinate pair; returns index to be used for addTriangle
		virtual bool smoothMesh(unsigned int id, double angle); //!< smooth vertex normals of mesh with given ID and angle (in degrees)
		// functions to build paramMaps instead of passing them from Blender
		// (decouling implementation details of STL containers, paraMap_t etc. as much as possible)
		virtual void paramsSetPoint(const char* name, double x, double y, double z);
		virtual void paramsSetString(const char* name, const char* s);
		virtual void paramsSetBool(const char* name, bool b);
		virtual void paramsSetInt(const char* name, int i);
		virtual void paramsSetFloat(const char* name, double f);
		virtual void paramsSetColor(const char* name, float r, float g, float b, float a=1.f);
		virtual void paramsSetColor(const char* name, float *rgb, bool with_alpha=false);
		virtual void paramsSetMatrix(const char* name, float m[4][4], bool transpose=false);
		virtual void paramsSetMatrix(const char* name, double m[4][4], bool transpose=false);
		virtual void paramsSetMemMatrix(const char* name, float* matrix, bool transpose=false);
		virtual void paramsSetMemMatrix(const char* name, double* matrix, bool transpose=false);
		virtual void paramsClearAll(); 	//!< clear the paramMap and paramList
		virtual void paramsStartList(); //!< start writing parameters to the extended paramList (used by materials)
		virtual void paramsPushList(); 	//!< push new list item in paramList (e.g. new shader node description)
		virtual void paramsEndList(); 	//!< revert to writing to normal paramMap
		// functions directly related to renderEnvironment_t
		virtual light_t* 		createLight			(const char* name);
		virtual texture_t* 		createTexture		(const char* name);
		virtual material_t* 	createMaterial		(const char* name);
		virtual camera_t* 		createCamera		(const char* name);
		virtual background_t* 	createBackground	(const char* name);
		virtual integrator_t* 	createIntegrator	(const char* name);
		virtual VolumeRegion* 	createVolumeRegion	(const char* name);
		virtual imageHandler_t*	createImageHandler	(const char* name);
		virtual unsigned int 	createObject		(const char* name);
		virtual void clearAll(); //!< clear the whole environment + scene, i.e. free (hopefully) all memory.
		virtual void render(colorOutput_t &output, progressBar_t *pb = 0); //!< render the scene...
		virtual bool startScene(int type=0); //!< start a new scene; Must be called before any of the scene_t related callbacks!
		virtual void setInputGamma(float gammaVal, bool enable);
		virtual void abort();
		virtual paraMap_t* getRenderParameters() { return params; }
		virtual bool getRenderedImage(colorOutput_t &output); //!< put the rendered image to output
		virtual std::vector<std::string> listImageHandlers();
		virtual std::vector<std::string> listImageHandlersFullName();
		virtual std::string getImageFormatFromFullName(const std::string &fullname);
		virtual std::string getImageFullNameFromFormat(const std::string &format);

		virtual void setDrawParams(bool on = true);
		virtual bool getDrawParams();

		//Versioning stuff
		virtual char* getVersion() const;
		
		//Console Printing wrappers to report in color with yafaray's own coloring mechanic
		void printInfo(const std::string &msg);
		void printWarning(const std::string &msg);
		void printError(const std::string &msg);
	
	protected:
		paraMap_t *params;
		std::list<paraMap_t> *eparams; //! for materials that need to define a whole shader tree etc.
		paraMap_t *cparams; //! just a pointer to the current paramMap, either params or a eparams element
		renderEnvironment_t *env;
		scene_t *scene;
		imageFilm_t *film;
		float inputGamma;
		bool gcInput;
};


class xmlInterface_t: public yafrayInterface_t
{
	public:
		xmlInterface_t();
		// directly related to scene_t:
		virtual void loadPlugins(const char *path);
		virtual bool startGeometry();
		virtual bool endGeometry();
		virtual unsigned int getNextFreeID();
		virtual bool startTriMesh(unsigned int id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool startCurveMesh(unsigned int id, int vertices);
		virtual bool startTriMeshPtr(unsigned int *id, int vertices, int triangles, bool hasOrco, bool hasUV=false, int type=0);
		virtual bool endTriMesh();
		virtual bool endCurveMesh(const material_t *mat, float strandStart, float strandEnd, float strandShape); //!< end current mesh and return to geometry state
		virtual int  addVertex(double x, double y, double z);
		virtual int  addVertex(double x, double y, double z, double ox, double oy, double oz);
		virtual bool addTriangle(int a, int b, int c, const material_t *mat);
		virtual bool addTriangle(int a, int b, int c, int uv_a, int uv_b, int uv_c, const material_t *mat);
		virtual int  addUV(float u, float v);
		virtual bool startVmap(int id, int type, int dimensions);
		virtual bool endVmap();
		virtual bool addVmapValues(float *val);
		virtual bool smoothMesh(unsigned int id, double angle);
		
		// functions directly related to renderEnvironment_t
		virtual light_t* 		createLight		(const char* name);
		virtual texture_t* 		createTexture	(const char* name);
		virtual material_t* 	createMaterial	(const char* name);
		virtual camera_t* 		createCamera	(const char* name);
		virtual background_t* 	createBackground(const char* name);
		virtual integrator_t* 	createIntegrator(const char* name);
		virtual unsigned int 	createObject	(const char* name);
		virtual void clearAll(); //!< clear the whole environment + scene, i.e. free (hopefully) all memory.
		virtual void render(colorOutput_t &output); //!< render the scene...
		virtual bool startScene(int type=0); //!< start a new scene; Must be called before any of the scene_t related callbacks!
		
		virtual void setOutfile(const char *fname);
	protected:
		void writeParamMap(const paramMap_t &pmap, int indent=1);
		void writeParamList(int indent);
		
		std::map<const material_t *, std::string> materials;
		std::ofstream xmlFile;
		std::string xmlName;
		const material_t *last_mat;
		size_t nmat;
		int n_uvs;
		unsigned int nextObj;
};

class memoryIO_t : public colorOutput_t
{
	public:
		memoryIO_t(int resx, int resy, float* iMem);
		virtual bool putPixel(int x, int y, const float *c, bool alpha = true, bool depth = false, float z = 0.f);
		void flush();
		virtual void flushArea(int x0, int y0, int x1, int y1) {}; // no tiled file format used...yet
		virtual ~memoryIO_t();
	protected:
		int sizex, sizey;
		float* imageMem;
};

}
