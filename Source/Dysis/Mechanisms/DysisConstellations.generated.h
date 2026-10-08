// 由 Tools/greybox/gen_constellations.py 生成，不要手改。
// 结局的两个星座：每颗星在小图上的位置（X 右、Y 上，最远的一颗离中心 = 1）和视星等；连线按画出来的顺序排。
#pragma once

#include "CoreMinimal.h"

struct FDysisConStar { float X, Y, Mag; };
struct FDysisConLine { int32 A, B; };

/** 双子座：16 颗星、15 条线（真的天上横竖大约 24°）。 */
inline const FDysisConStar GDysisGeminiStars[] = {
	{   0.1122f,   0.8590f, 1.58f },   // Castor
	{  -0.2596f,   0.8269f, 1.14f },   // Pollux
	{  -0.1919f,  -0.7763f, 1.93f },   // Alhena
	{  -0.3739f,   0.1494f, 3.53f },   // Wasat
	{   0.2598f,  -0.2029f, 3.06f },   // Mebsuta
	{  -0.2706f,  -0.1601f, 3.90f },   // Mekbuda
	{   0.4828f,  -0.7533f, 3.30f },   // Propus
	{   0.3728f,  -0.6403f, 2.87f },   // Tejat
	{  -0.4939f,   0.6376f, 3.57f },   // Kappa
	{  -0.7006f,  -0.1598f, 3.58f },   // Lambda
	{  -0.5119f,  -0.8590f, 3.35f },   // Alzirr
	{   0.1553f,  -0.6872f, 4.15f },   // Nu
	{   0.2574f,   0.4451f, 4.40f },   // Tau
	{  -0.2339f,   0.6309f, 4.06f },   // Upsilon
	{  -0.0625f,   0.5257f, 3.78f },   // Iota
	{   0.7006f,   0.3993f, 3.60f },   // Theta
};
inline const FDysisConLine GDysisGeminiLines[] = { { 0, 12 }, { 12, 4 }, { 4, 7 }, { 7, 6 }, { 4, 11 }, { 12, 15 }, { 1, 13 }, { 13, 3 }, { 3, 5 }, { 5, 2 }, { 3, 9 }, { 9, 10 }, { 1, 8 }, { 12, 14 }, { 14, 13 } };

/** 天鹅座：9 颗星、8 条线（真的天上横竖大约 36°）。 */
inline const FDysisConStar GDysisCygnusStars[] = {
	{  -0.0104f,   0.6135f, 1.25f },   // Deneb
	{  -0.0581f,   0.2830f, 2.23f },   // Sadr
	{  -0.4853f,   0.2141f, 2.48f },   // Gienah
	{   0.3963f,   0.2579f, 2.87f },   // Fawaris
	{  -0.0104f,  -0.6135f, 3.05f },   // Albireo
	{  -0.0231f,  -0.1110f, 3.89f },   // Eta
	{  -0.8621f,   0.3034f, 3.21f },   // Zeta
	{   0.7192f,   0.4823f, 3.76f },   // Iota
	{   0.8621f,   0.5067f, 3.80f },   // Kappa
};
inline const FDysisConLine GDysisCygnusLines[] = { { 0, 1 }, { 1, 5 }, { 5, 4 }, { 1, 2 }, { 2, 6 }, { 1, 3 }, { 3, 7 }, { 7, 8 } };
