// Copyright KawaiiFluid Team. All Rights Reserved.

#include "Physics/SPHKernels.h"

namespace SPHKernels
{
	//========================================
	// Poly6 커널
	//========================================

	float Poly6Coefficient(float h)
	{
		// 315 / (64 * π * h^9)
		return 315.0f / (64.0f * PI * FMath::Pow(h, 9.0f));
	}

	float Poly6(float r, float h)
	{
		if (r < 0.0f || r > h)
		{
			return 0.0f;
		}

		float h2 = h * h;
		float r2 = r * r;
		float diff = h2 - r2;

		return Poly6Coefficient(h) * diff * diff * diff;
	}

	float Poly6(const FVector& r, float h)
	{
		return Poly6(r.Size(), h);
	}

	//========================================
	// Spiky 커널 그래디언트
	//========================================

	float SpikyGradientCoefficient(float h)
	{
		// -45 / (π * h^6)
		return -45.0f / (PI * FMath::Pow(h, 6.0f));
	}

	FVector SpikyGradient(const FVector& r, float h)
	{
		float rLen = r.Size();

		if (rLen <= 0.0f || rLen > h)
		{
			return FVector::ZeroVector;
		}

		float diff = h - rLen;
		float coeff = SpikyGradientCoefficient(h) * diff * diff;

		// r̂ (단위 벡터)
		FVector rNorm = r / rLen;

		return coeff * rNorm;
	}

	//========================================
	// Viscosity 커널 라플라시안
	//========================================

	float ViscosityLaplacianCoefficient(float h)
	{
		// 45 / (π * h^6)
		return 45.0f / (PI * FMath::Pow(h, 6.0f));
	}

	float ViscosityLaplacian(float r, float h)
	{
		if (r < 0.0f || r > h)
		{
			return 0.0f;
		}

		return ViscosityLaplacianCoefficient(h) * (h - r);
	}

	//========================================
	// Adhesion 커널 (Akinci 2013)
	//========================================

	float Adhesion(float r, float h)
	{
		// 유효 범위: 0.5h < r < h
		if (r < 0.5f * h || r > h)
		{
			return 0.0f;
		}

		// 0.007 / h^3.25 * (-4r²/h + 6r - 2h)^0.25
		float coeff = 0.007f / FMath::Pow(h, 3.25f);
		float inner = -4.0f * r * r / h + 6.0f * r - 2.0f * h;

		if (inner <= 0.0f)
		{
			return 0.0f;
		}

		return coeff * FMath::Pow(inner, 0.25f);
	}

	//========================================
	// Cohesion 커널 (Akinci 2013)
	//========================================

	float Cohesion(float r, float h)
	{
		if (r < 0.0f || r > h)
		{
			return 0.0f;
		}

		float coeff = 32.0f / (PI * FMath::Pow(h, 9.0f));
		float h2 = h * 0.5f;

		if (r <= h2)
		{
			// 0 < r <= h/2
			float diff1 = h - r;
			float diff2 = diff1 * diff1 * diff1;
			float r3 = r * r * r;
			return coeff * 2.0f * diff2 * r3 - (FMath::Pow(h, 6.0f) / 64.0f);
		}
		else
		{
			// h/2 < r <= h
			float diff = h - r;
			return coeff * diff * diff * diff * r * r * r;
		}
	}

	//========================================
	// 계수 미리 계산
	//========================================

	void FKernelCoefficients::Precompute(float SmoothingRadius)
	{
		h = SmoothingRadius;
		h2 = h * h;
		h6 = h2 * h2 * h2;
		h9 = h6 * h2 * h;

		Poly6Coeff = 315.0f / (64.0f * PI * h9);
		SpikyGradCoeff = -45.0f / (PI * h6);
		ViscosityLapCoeff = 45.0f / (PI * h6);
	}
}
