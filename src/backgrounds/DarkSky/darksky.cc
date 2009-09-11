/********************************************************************************
 *
 * darksky.cc: SkyLight, "Real" Sunlight and Sky Background
 *
 *		Created on: 20/03/2009
 *
 *  	Based on the original implementation by Mathias Wein (Lynx), anyone else???
 *  	Actual implementation by Rodrigo Placencia (Darktide)
 *
 * 		Based on 'A Practical Analytic Model For DayLight" by Preetham, Shirley & Smits.
 * 		http://www.cs.utah.edu/vissim/papers/sunsky/
 * 		based on the actual code by Brian Smits
 *
 */

#include <yafray_config.h>

#include <core_api/environment.h>
#include <core_api/background.h>
#include <core_api/params.h>
#include <lights/bglight.h>

#include "ColorConv.h"
#include "spectralData.h"
#include "sunSpectrumCurves.h"

__BEGIN_YAFRAY

class darkSkyBackground_t: public background_t
{
	public:
		darkSkyBackground_t(const point3d_t dir, PFLOAT turb, bool bgl, int bgsamples, CFLOAT pwr, PFLOAT skyBright, bool clamp,
						   float av, float bv, float cv, float dv, float ev, PFLOAT altitude, bool night);
		virtual color_t operator() (const ray_t &ray, renderState_t &state, bool filtered=false) const;
		virtual color_t eval(const ray_t &ray, bool filtered=false) const;
		virtual light_t* getLight() const { return envLight; }
		virtual ~darkSkyBackground_t();
		static background_t *factory(paraMap_t &,renderEnvironment_t &);
		color_t getAttenuatedSunColor();

	protected:
		color_t getSkyCol(const ray_t &ray) const;
		double PerezFunction(const double *lam, double cosTheta, double gamma, double cosGamma, double lvz) const;
		double prePerez(const double *perez);

		color_t getSunColorFromPerez();
		color_t getSunColorFromSunRad();

		vector3d_t sunDir;
		double thetaS;
		double theta2, theta3;
		double sinThetaS, cosThetaS, cosTheta2;
		double T, T2;
		double zenith_Y, zenith_x, zenith_y;
		double perez_Y[6], perez_x[6], perez_y[6];
		light_t *envLight;
		CFLOAT power;
		PFLOAT skyBrightness;
		ColorConv convert;
		PFLOAT alt;
		bool nightSky;
};

darkSkyBackground_t::darkSkyBackground_t(const point3d_t dir, PFLOAT turb, bool bgl, int bgsamples, CFLOAT pwr, PFLOAT skyBright, bool clamp,
									   float av, float bv, float cv, float dv, float ev, PFLOAT altitude, bool night):
									   envLight(0), power(pwr), skyBrightness(skyBright), convert(clamp), alt(altitude), nightSky(night)
{
	
	
	std::string act = "";

	sunDir = vector3d_t(dir);
	sunDir.z += alt;
	sunDir.normalize();

	thetaS = acos(sunDir.z);
	
	act = (nightSky)?"ON":"OFF";
	Y_INFO << "DarkSky: Night mode [ " << act << " ]" << std::endl;
	Y_INFO << "DarkSky: Solar Declination in Degrees (" << radToDeg(thetaS) << ")" << std::endl;
	act = (clamp)?"active.":"inactive.";
	Y_INFO << "DarkSky: RGB Clamping " << act << std::endl;
	Y_INFO << "DarkSky: Altitude " << alt << std::endl;

	cosThetaS = cos(thetaS);
	cosTheta2 = cosThetaS * cosThetaS;
	sinThetaS = sin(thetaS);

	theta2 = thetaS*thetaS;
	theta3 = theta2*thetaS;

	T = turb;
	T2 = turb*turb;

	double chi = ((53.33333333f - T) / 120.0) * (M_PI - (2.0 * thetaS));

	zenith_Y = ((4.0453 * T - 4.9710) * tan(chi)) - (0.2155 * T + 2.4192);
	zenith_Y *= 1000;  // conversion from kcd/m^2 to cd/m^2

	zenith_x =
	( 0.00165*theta3 - 0.00374*theta2 + 0.00209*thetaS          ) * T2 +
	(-0.02902*theta3 + 0.06377*theta2 - 0.03202*thetaS + 0.00394) * T  +
	( 0.11693*theta3 - 0.21196*theta2 + 0.06052*thetaS + 0.25885);

	zenith_y =
	( 0.00275*theta3 - 0.00610*theta2 + 0.00316*thetaS          ) * T2 +
	(-0.04214*theta3 + 0.08970*theta2 - 0.04153*thetaS + 0.00515) * T  +
	( 0.15346*theta3 - 0.26756*theta2 + 0.06669*thetaS + 0.26688);

	perez_Y[0] = (( 0.17872 * T) - 1.46303) * av;
	perez_Y[1] = ((-0.35540 * T) + 0.42749) * bv;
	perez_Y[2] = ((-0.02266 * T) + 5.32505) * cv;
	perez_Y[3] = (( 0.12064 * T) - 2.57705) * dv;
	perez_Y[4] = ((-0.06696 * T) + 0.37027) * ev;
	perez_Y[5] = prePerez(perez_Y);

	perez_x[0] = ((-0.01925 * T) - 0.25922);
	perez_x[1] = ((-0.06651 * T) + 0.00081);
	perez_x[2] = ((-0.00041 * T) + 0.21247);
	perez_x[3] = ((-0.06409 * T) - 0.89887);
	perez_x[4] = ((-0.00325 * T) + 0.04517);
	perez_x[5] = prePerez(perez_x);

	perez_y[0] = ((-0.01669 * T) - 0.26078);
	perez_y[1] = ((-0.09495 * T) + 0.00921);
	perez_y[2] = ((-0.00792 * T) + 0.21023);
	perez_y[3] = ((-0.04405 * T) - 1.65369);
	perez_y[4] = ((-0.01092 * T) + 0.05291);
	perez_y[5] = prePerez(perez_y);

	if(bgl) envLight = new bgLight_t(this, bgsamples);
};

color_t darkSkyBackground_t::getAttenuatedSunColor()
{
	color_t lightColor(1.0);
	
	if(thetaS > degToRad(70))
	{
		lightColor = getSunColorFromSunRad() * getSunColorFromPerez();
	}
	else
	{
		lightColor = getSunColorFromSunRad();
	}
	
	if(nightSky)
	{
		lightColor *= color_t(0.8,0.8,1.0);
	}

	return lightColor;
}

color_t darkSkyBackground_t::getSunColorFromPerez()
{
	double pCosTheta = (thetaS > M_PI_2)?0.0:cosThetaS;
	color_t sunColor = convert.fromxyY
	(
		PerezFunction(perez_x, pCosTheta, 0.0, 1.0, zenith_x),
		PerezFunction(perez_y, pCosTheta, 0.0, 1.0, zenith_y),
		PerezFunction(perez_Y, pCosTheta, 0.0, 1.0, zenith_Y)
	);
	return (sunColor / sunColor.maximum()) * 0.5; //this is to let the color be affected by power
}

color_t darkSkyBackground_t::getSunColorFromSunRad()
{
	int L, uL;
	float kgLm, kwaLmw, mw;
    float Rayleigh, Angstrom, Ozone, Gas, Water, m, lm, m1, mB, am, m4;
	color_t sXYZ(0.0);
	color_t tmpCol(0.0);

	float B = (0.04608365822050 * T) - 0.04586025928522;
	float a = 1.3;
	float l = 0.35;
	float w = 2.0;

	IrregularCurve ko(koAmplitudes, koWavelengths, 64);
    IrregularCurve kg(kgAmplitudes, kgWavelengths, 4);
    IrregularCurve kwa(kwaAmplitudes, kwaWavelengths, 13);
    RegularCurve sunRadianceCurve(sunRadiance, 380, 750, 38);

    m = 1.0/(cosThetaS + 0.15 * fPow(93.885 - radToDeg(thetaS),-1.253));
	mw = m * w;
	lm = -m * l;
	
	if(nightSky)
	{
		m1 = -0.008735;
		mB = -B;
		am = -a * m;
		m4 = -4.08 * m;
	}
	else
	{
		m1 = -0.008735 * m;
		mB = -B * m;
		am = -a;
		m4 = -4.08;
	}

	for(L = 360; L < 835; L+=5)
	{
		uL = L * 1000;
		kgLm = kg(L) * m;
		kwaLmw = kwa(L) * mw;

		Rayleigh = fExp(m1 * fPow(uL, m4));
		Angstrom = fExp(mB * fPow(uL, am));
		Ozone = fExp(ko(L) * lm);
		Gas = fExp((-1.41 * kgLm) / fPow(1 + (118.93 * kgLm), 0.45));
		Water = fExp((-0.2385 * kwaLmw) / fPow(1 + (20.07 * kwaLmw), 0.45));

		tmpCol = chromaMatch(L) * sunRadianceCurve(L) * Rayleigh * Angstrom * Ozone * Gas * Water;
		sXYZ = 1.0 - ((1.0 - sXYZ)*(1.0 - convert.fromXYZ(tmpCol)));
	}

	return sXYZ;
}

darkSkyBackground_t::~darkSkyBackground_t()
{
	if(envLight) delete envLight;
}

double darkSkyBackground_t::prePerez(const double *perez)
{
	return 1.0 / ((1 + perez[0] * fExp(perez[1])) * (1 +( perez[2] * fExp(perez[3] * thetaS) ) + (perez[4] * cosTheta2)));
}

double darkSkyBackground_t::PerezFunction(const double *lam, double cosTheta, double gamma, double cosGamma2, double lvz) const
{
  double num = ( (1 + lam[0] * fExp(lam[1]/cosTheta) ) * (1 + lam[2] * fExp(lam[3]*gamma)  + lam[4] * cosGamma2));

  return lvz * num * lam[5];
}

inline color_t darkSkyBackground_t::getSkyCol(const ray_t &ray) const
{
	vector3d_t Iw = ray.dir;
	Iw.z += alt;
	Iw.normalize();

	double theta, cosTheta, gamma, cosGamma, cosGamma2;
	double x, y, Y;
	color_t skyCol(0.0);
	
	cosTheta = Iw.z;
	
	if(cosTheta < 0.0)
	{
		theta = M_PI_2;
		cosTheta = 0.0;
	}
	else
	{
		theta = acos(cosTheta);
	}
    
	cosGamma = Iw VDOT sunDir;
    cosGamma2 = cosGamma * cosGamma;
	gamma = acos(cosGamma);

	x = PerezFunction(perez_x, cosTheta, gamma, cosGamma2, zenith_x);
	y = PerezFunction(perez_y, cosTheta, gamma, cosGamma2, zenith_y);
	Y = PerezFunction(perez_Y, cosTheta, gamma, cosGamma2, zenith_Y);
	
	skyCol = convert.fromxyY(x,y,Y);
	
	if(nightSky) skyCol *= color_t(0.05,0.05,0.08);
	
	return skyCol;
}

color_t darkSkyBackground_t::operator() (const ray_t &ray, renderState_t &state, bool filtered) const
{
	color_t ret = getSkyCol(ray) * skyBrightness;
	ret.clampRGB01();
	return ret;
}

color_t darkSkyBackground_t::eval(const ray_t &ray, bool filtered) const
{
	color_t ret = getSkyCol(ray) * power;
	ret.clampRGB01();
	return ret;
}

background_t *darkSkyBackground_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	point3d_t dir(1,1,1);
	CFLOAT turb = 4.0;
	PFLOAT altitude = 0.0;
	int bgl_samples = 8;
	PFLOAT power = 1.0; //bgLight Power
	PFLOAT pw = 1.0;// sunlight power
	PFLOAT bright = 1.0;
	bool add_sun = false;
	bool bgl = false;
	bool clamp = false;
	bool night = false;
	float av, bv, cv, dv, ev;
	av = bv = cv = dv = ev = 1.0;

	Y_INFO << "DarkSky: Begin" << std::endl;

	params.getParam("from", dir);
	params.getParam("turbidity", turb);
	params.getParam("altitude", altitude);
	params.getParam("power", power);
	params.getParam("bright", bright);

	params.getParam("clamp_rgb", clamp);

	params.getParam("a_var", av); //Darkening or brightening towards horizon
	params.getParam("b_var", bv); //Luminance gradient near the horizon
	params.getParam("c_var", cv); //Relative intensity of circumsolar region
	params.getParam("d_var", dv); //Width of circumsolar region
	params.getParam("e_var", ev); //Relative backscattered light

	params.getParam("add_sun", add_sun);
	params.getParam("sun_power", pw);

	params.getParam("background_light", bgl);
	params.getParam("light_samples", bgl_samples);

	params.getParam("night", night);
	
	if(night)
	{
		bright *= 0.5;
		pw *= 0.5;
	}

	darkSkyBackground_t * new_sunsky = new darkSkyBackground_t(dir, turb, bgl, bgl_samples, power, bright, clamp, av, bv, cv, dv, ev, altitude, night);

	if (add_sun && radToDeg(acos(dir.z)) < 100.0)
	{
		vector3d_t d(dir);
		d.normalize();

		color_t suncol = new_sunsky->getAttenuatedSunColor();
		double angle = 0.5 * (2.0 - d.z);

		Y_INFO << "DarkSky: SunColor = " << suncol << std::endl;

		paraMap_t p;
		p["type"] = std::string("sunlight");
		p["direction"] = point3d_t(d);
		p["color"] = suncol;
		p["angle"] = parameter_t(angle);
		p["power"] = parameter_t(pw);
		p["samples"] = bgl_samples;

		Y_INFO << "DarkSky: Adding a \"Real Sun\"";

		light_t *light = render.createLight("DarkSky_RealSun", p);
		if(light)
		{
			render.getScene()->addLight(light);
		}
	}

	Y_INFO << "DarkSky: End" << std::endl;

	return (background_t *)new_sunsky;
}

extern "C"
{

	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("darksky",darkSkyBackground_t::factory);
	}

}
__END_YAFRAY