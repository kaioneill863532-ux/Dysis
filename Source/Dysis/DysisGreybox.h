// 日落回廊 · 灰盒 v0.12（prototype/temple/index.html）里的尺寸常数，换成 UE 的厘米。
// 灰盒坐标：x 东、y 上、z 南（米）；UE：X 北、Y 东、Z 上（厘米）。方位角：北 0°，顺时针（东 90°）。
// 逻辑和数值以灰盒为准：这里的数不要按手感改，要改先改灰盒。
#pragma once

#include "CoreMinimal.h"

namespace DysisGB
{
	// ── 殿的半径（厘米） ──
	constexpr float R_POOL = 1020.0f;                    // 水池边
	constexpr float R_IN = 1550.0f, R_OUT = 1710.0f;     // 内墙面、外墙面
	constexpr float TUN_R0 = 1580.0f, TUN_R1 = 1680.0f;  // 墙里的楼梯（1 m 宽）
	// ── 高度（厘米） ──
	constexpr float WATER_Z = -45.0f;                    // 水庭水面
	constexpr float SEA_Z = -1600.0f;                    // 海面
	// 开局站的那一块的顶面。灰盒里是一座高出海面 4 m 的小岛（−1200）。2026-10-11 用户：海面改成镜湖以后那个台子不要了，人直接出生在水面上，
	// 出生点 = 开场那束光延长到海面的地方。——岛不画了，只剩挡人、接光的那一层，整个顺着那束光往外、往下挪到顶面和海面一样高
	// （IsleMoveCm；ADysisGameMode::InitGame 里连岛边的护栏、出生点一起挪）。人还是只能在这一块（半径 4.5 m）上走，走不出去。
	constexpr float ISLAND_TOP_Z = SEA_Z;
	// ── 接缝（瀑布那一段弧，度） ──
	constexpr float SEAM0 = 116.0f, SEAM1 = 140.0f;

	// ── 开局：站在水面上，面朝神殿。灰盒 SPAWN 是岛外沿的 (4978.79, 2189.09, −1200)；
	//    顺着开场那束光（开局那一刻太阳高 30.1°，光的方向 (0.7887, 0.3553, −0.5016)）再走 7.97 m 就到海面，就是这里 ──
	inline const FVector SpawnFootCm(5607.72f, 2472.40f, -1600.0f);
	inline const FVector IsleMoveCm(628.93f, 283.31f, -400.0f);       // 岛、岛边的护栏、出生点一起挪这么多
	inline const FVector2D IsleWasCenterCm(4815.0f, 2114.0f);         // 岛顶面的圆心（挪之前）
	constexpr float SpawnYawDeg = 203.7343f;             // 朝向的方位角；UE 里 Yaw 就是这个数

	/** 方位角（度，0–360）：北 0，顺时针。 */
	inline float AzOf(const FVector& P)
	{
		const float A = FMath::RadiansToDegrees(FMath::Atan2(float(P.Y), float(P.X)));
		return A < 0.0f ? A + 360.0f : A;
	}
	/** 灰盒的 P(az, r, y)：方位角（度）、半径、高度 → UE 厘米。 */
	inline FVector PolarCm(double AzDeg, double R, double Z)
	{
		const double A = FMath::DegreesToRadians(AzDeg);
		return FVector(R * FMath::Cos(A), R * FMath::Sin(A), Z);
	}
	/** 两个方位角差多少（−180…180，度）。灰盒 angDiff。 */
	inline double AngDiff(double A, double B) { return FMath::Fmod(FMath::Fmod(A - B, 360.0) + 540.0, 360.0) - 180.0; }
	/** 离殿中轴的水平距离（厘米）。 */
	inline float ROf(const FVector& P) { return FMath::Sqrt(float(P.X * P.X + P.Y * P.Y)); }
	/** 方位角 Az 在不在 [A0, A1] 这段弧里（灰盒 inArc）。 */
	inline bool InArc(float Az, float A0, float A1)
	{
		float T = Az;
		while (T < A0) T += 360.0f;
		while (T >= A0 + 360.0f) T -= 360.0f;
		return T <= A1;
	}
	/** 灰盒的 toward：以每秒 Rate 的速度把 V 挪向 Target。 */
	inline float Toward(float V, float Target, float Rate, float Dt)
	{
		return V < Target ? FMath::Min(Target, V + Rate * Dt) : FMath::Max(Target, V - Rate * Dt);
	}
	inline float Smoothstep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}
}
