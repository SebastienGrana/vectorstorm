/*
 *  VS_Rift.h
 *  VectorStorm
 *
 *  Created by Trevor Powell on 15-07-2013.
 *  Copyright 2013 Trevor Powell.  All rights reserved.
 *
 */
#ifndef VS_RIFT_H
#define VS_RIFT_H

#include "VS/Utils/VS_Singleton.h"
#include "VS/Math/VS_Vector.h"

// Management class for an Oculus Rift device.  This handles everything
// to do with the rift, from detection to input to basic rendering data
// (which eventually gets used elsewhere inside VectorStorm -- hopefully
// in a way this will eventually support other HMDs, if they pop up)

class vsRift: public vsSingleton<vsRift>
{
	vsString m_monitorName;
	float m_eyeDistance;
	float m_distortionK[4];
	bool m_hasHmd;
public:
	vsRift();
	~vsRift();

	bool HasRift() const { return m_hasHmd; }

	float GetLensSeparationDistance() const;
	float GetHScreenSize() const;
	float GetVScreenSize() const;
	float GetDistanceBetweenPupils() const;
	vsVector4D GetDistortionK() const;
};

#endif /* VS_RIFT_H */

