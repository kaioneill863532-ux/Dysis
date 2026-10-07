// 最小假 UE：只为在没有引擎的机器上做 clang -fsyntax-only 类型检查（签名按 UE 5.x 写；以真 UE 为准）。check_cpp.sh 用它。
#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>
#include <initializer_list>
#include <type_traits>
#include <utility>

#define WITH_EDITOR 1
#define ENABLE_DRAW_DEBUG 0
#define UE_BUILD_SHIPPING 0
#define DYSIS_API
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UINTERFACE(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UMETA(...)
#define GENERATED_BODY() public:
#define TEXT(x) u##x
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define KINDA_SMALL_NUMBER (1.e-4f)
#define UE_DOUBLE_PI (3.141592653589793)
#define UE_PI (3.14159265f)
#define GET_MEMBER_NAME_STRING_CHECKED(C, M) ((void)sizeof(((C*)nullptr)->M), TEXT(#M))
#define RETURN_QUICK_DECLARE_CYCLE_STAT(a, b) return TStatId();

typedef int8_t int8; typedef uint8_t uint8; typedef int16_t int16; typedef uint16_t uint16;
typedef int32_t int32; typedef uint32_t uint32; typedef int64_t int64; typedef uint64_t uint64;
typedef char16_t TCHAR; typedef char ANSICHAR;

namespace ESearchCase { enum Type { CaseSensitive, IgnoreCase }; }

class FString
{
public:
	std::u16string S;
	FString() {}
	FString(const TCHAR* In) : S(In ? In : u"") {}
	FString(const FString&) = default;
	FString& operator=(const FString&) = default;
	const TCHAR* operator*() const { return S.c_str(); }
	int32 Len() const { return (int32)S.size(); }
	bool IsEmpty() const { return S.empty(); }
	FString& operator+=(const FString& O) { S += O.S; return *this; }
	FString& operator+=(const TCHAR* O) { S += O; return *this; }
	friend FString operator+(const FString& A, const FString& B) { FString R(A); R.S += B.S; return R; }
	friend FString operator+(const FString& A, const TCHAR* B) { FString R(A); R.S += B; return R; }
	friend FString operator+(const TCHAR* A, const FString& B) { FString R(A); R.S += B.S; return R; }
	bool operator==(const FString& O) const { return S == O.S; }
	bool operator!=(const FString& O) const { return S != O.S; }
	bool operator<(const FString& O) const { return S < O.S; }
	bool Contains(const FString& Sub, ESearchCase::Type = ESearchCase::IgnoreCase) const { return S.find(Sub.S) != std::u16string::npos; }
	bool StartsWith(const FString& P, ESearchCase::Type = ESearchCase::IgnoreCase) const { return S.rfind(P.S, 0) == 0; }
	bool Equals(const FString& O, ESearchCase::Type = ESearchCase::CaseSensitive) const { return S == O.S; }
	template <typename... A> static FString Printf(const TCHAR*, A&&...) { return FString(); }
};
inline uint32 GetTypeHash(const FString& S) { return (uint32)std::hash<std::u16string>()(S.S); }

enum EName { NAME_None };
class FName
{
public:
	FString N;
	FName() {}
	FName(EName) {}
	FName(const TCHAR* In) : N(In) {}
	FName(const FString& In) : N(In) {}
	FString ToString() const { return N; }
	bool IsNone() const { return N.IsEmpty(); }
	bool operator==(const FName& O) const { return N == O.N; }
	bool operator!=(const FName& O) const { return !(N == O.N); }
	bool operator<(const FName& O) const { return N < O.N; }
};
inline uint32 GetTypeHash(const FName& N) { return GetTypeHash(N.N); }

class FText
{
public:
	FString T;
	static FText FromString(const FString& S) { FText R; R.T = S; return R; }
	static const FText& GetEmpty() { static FText E; return E; }
	FString ToString() const { return T; }
	bool IsEmpty() const { return T.IsEmpty(); }
};

namespace FCString
{
	inline float Atof(const TCHAR*) { return 0.f; }
	inline int32 Atoi(const TCHAR*) { return 0; }
}

template <typename K, typename V> struct TPair { K Key; V Value; };

template <typename T>
class TArray
{
public:
	std::deque<T> V;
	TArray() {}
	TArray(std::initializer_list<T> L) : V(L) {}
	TArray& operator=(std::initializer_list<T> L) { V = L; return *this; }
	int32 Num() const { return (int32)V.size(); }
	bool IsValidIndex(int32 i) const { return i >= 0 && i < Num(); }
	T& operator[](int32 i) { return V[i]; }
	const T& operator[](int32 i) const { return V[i]; }
	int32 Add(const T& X) { V.push_back(X); return Num() - 1; }
	int32 AddUnique(const T& X) { for (int32 i = 0; i < Num(); ++i) if (V[i] == X) return i; return Add(X); }
	void RemoveAt(int32 i) { V.erase(V.begin() + i); }
	int32 Remove(const T& X) { auto n = V.size(); V.erase(std::remove(V.begin(), V.end(), X), V.end()); return (int32)(n - V.size()); }
	template <typename P> int32 RemoveAll(P Pred) { auto n = V.size(); V.erase(std::remove_if(V.begin(), V.end(), Pred), V.end()); return (int32)(n - V.size()); }
	template <typename P> bool ContainsByPredicate(P Pred) const { return std::any_of(V.begin(), V.end(), Pred); }
	bool Contains(const T& X) const { return std::find(V.begin(), V.end(), X) != V.end(); }
	void Reset() { V.clear(); }
	void Empty() { V.clear(); }
	void SetNum(int32 N) { V.resize(N); }
	void Init(const T& X, int32 N) { V.assign(N, X); }
	template <typename P> void Sort(P Pred) { (void)Pred; }
	typename std::deque<T>::iterator begin() { return V.begin(); }
	typename std::deque<T>::iterator end() { return V.end(); }
	typename std::deque<T>::const_iterator begin() const { return V.begin(); }
	typename std::deque<T>::const_iterator end() const { return V.end(); }
};
// UE 的指针数组 Sort：谓词拿到的是解引用后的对象。
template <typename T>
class TArray<T*>
{
public:
	std::vector<T*> V;
	TArray() {}
	int32 Num() const { return (int32)V.size(); }
	T*& operator[](int32 i) { return V[i]; }
	int32 Add(T* X) { V.push_back(X); return Num() - 1; }
	template <typename P> void Sort(P Pred) { std::sort(V.begin(), V.end(), [&](T* A, T* B) { return Pred(*A, *B); }); }
	typename std::vector<T*>::iterator begin() { return V.begin(); }
	typename std::vector<T*>::iterator end() { return V.end(); }
};

template <typename T>
class TSet
{
public:
	std::vector<T> V;
	void Add(const T& X) { if (!Contains(X)) V.push_back(X); }
	bool Contains(const T& X) const { return std::find(V.begin(), V.end(), X) != V.end(); }
};

template <typename K, typename VT>
class TMap
{
public:
	std::vector<TPair<K, VT>> P;
	VT* Find(const K& Key) { for (auto& E : P) if (E.Key == Key) return &E.Value; return nullptr; }
	const VT* Find(const K& Key) const { for (auto& E : P) if (E.Key == Key) return &E.Value; return nullptr; }
	VT& FindOrAdd(const K& Key) { if (VT* F = Find(Key)) return *F; P.push_back({Key, VT()}); return P.back().Value; }
	VT& Add(const K& Key, const VT& Val) { P.push_back({Key, Val}); return P.back().Value; }
	int32 Remove(const K& Key) { auto n = P.size(); P.erase(std::remove_if(P.begin(), P.end(), [&](const TPair<K, VT>& E) { return E.Key == Key; }), P.end()); return (int32)(n - P.size()); }
	bool Contains(const K& Key) const { return Find(Key) != nullptr; }
	void Reset() { P.clear(); }
	int32 Num() const { return (int32)P.size(); }
	typename std::vector<TPair<K, VT>>::iterator begin() { return P.begin(); }
	typename std::vector<TPair<K, VT>>::iterator end() { return P.end(); }
	typename std::vector<TPair<K, VT>>::const_iterator begin() const { return P.begin(); }
	typename std::vector<TPair<K, VT>>::const_iterator end() const { return P.end(); }
};

template <typename T>
class TSharedPtr
{
public:
	std::shared_ptr<T> P;
	TSharedPtr() {}
	TSharedPtr(std::shared_ptr<T> In) : P(In) {}
	bool IsValid() const { return (bool)P; }
	void Reset() { P.reset(); }
	T* operator->() const { return P.get(); }
};

// ── 数学 ──
struct FVector
{
	double X = 0, Y = 0, Z = 0;
	FVector() {}
	FVector(double x, double y, double z) : X(x), Y(y), Z(z) {}
	static const FVector ZeroVector;
	static const FVector ForwardVector;
	static const FVector UpVector;
	FVector operator+(const FVector& O) const { return FVector(X + O.X, Y + O.Y, Z + O.Z); }
	FVector operator-(const FVector& O) const { return FVector(X - O.X, Y - O.Y, Z - O.Z); }
	FVector operator*(double S) const { return FVector(X * S, Y * S, Z * S); }
	FVector operator/(double S) const { return FVector(X / S, Y / S, Z / S); }
	FVector& operator+=(const FVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	double SizeSquared() const { return X * X + Y * Y + Z * Z; }
	bool IsNearlyZero(double T = 1e-4) const { return SizeSquared() < T * T; }
	bool IsZero() const { return X == 0 && Y == 0 && Z == 0; }
	bool Equals(const FVector& O, double T = 1e-4) const { return std::fabs(X - O.X) <= T && std::fabs(Y - O.Y) <= T && std::fabs(Z - O.Z) <= T; }
	FVector GetSafeNormal() const { return *this; }
	static double Dist(const FVector& A, const FVector& B) { return (A - B).Size(); }
	static double DistSquared(const FVector& A, const FVector& B) { return (A - B).SizeSquared(); }
	static double DistSquared2D(const FVector& A, const FVector& B) { return (A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y); }
	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static FVector CrossProduct(const FVector& A, const FVector& B) { return FVector(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
};
struct FVector2f
{
	float X = 0, Y = 0;
	FVector2f() {}
	FVector2f(float x, float y) : X(x), Y(y) {}
	bool Equals(const FVector2f& O, float T) const { return std::fabs(X - O.X) <= T && std::fabs(Y - O.Y) <= T; }
};
struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	FRotator() {}
	FRotator(double p, double y, double r) : Pitch(p), Yaw(y), Roll(r) {}
	static const FRotator ZeroRotator;
	bool Equals(const FRotator& O, double T = 1e-4) const { return std::fabs(Pitch - O.Pitch) <= T && std::fabs(Yaw - O.Yaw) <= T && std::fabs(Roll - O.Roll) <= T; }
	FVector Vector() const { return FVector(); }
	FVector RotateVector(const FVector& V) const { return V; }
};
struct FQuat
{
	double X = 0, Y = 0, Z = 0, W = 1;
	FQuat() {}
	FQuat(const FVector&, double) {}
	static const FQuat Identity;
	bool Equals(const FQuat& O, double T = 1e-4) const { return std::fabs(X - O.X) <= T && std::fabs(W - O.W) <= T; }
	FQuat operator*(const FQuat& O) const { return O; }
};
struct FTransform
{
	FVector L; FQuat Q; FVector S3;
	FVector GetLocation() const { return L; }
	FQuat GetRotation() const { return Q; }
	FVector GetScale3D() const { return S3; }
};
struct FColor { uint8 R = 0, G = 0, B = 0, A = 255; FColor() {} FColor(uint8 r, uint8 g, uint8 b, uint8 a = 255) : R(r), G(g), B(b), A(a) {} static const FColor Green, Red, Yellow, Cyan; };
struct FLinearColor { float R = 0, G = 0, B = 0, A = 1; FLinearColor() {} FLinearColor(float r, float g, float b, float a = 1.f) : R(r), G(g), B(b), A(a) {} static const FLinearColor Green; };

struct FMath
{
	template <typename T> static T Clamp(T X, T A, T B) { return X < A ? A : (X > B ? B : X); }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	template <typename T> static T Square(T A) { return A * A; }
	template <typename T, typename U> static T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + (B - A) * Alpha); }
	static float Pow(float A, float B) { return std::pow(A, B); }
	static float Fmod(float A, float B) { return std::fmod(A, B); }
	static float FInterpConstantTo(float Cur, float Target, float Dt, float Speed) { (void)Dt; (void)Speed; return Target + 0 * Cur; }
	static float FRandRange(float A, float B) { return (A + B) * 0.5f; }
	static int32 RandRange(int32 A, int32 B) { return A + 0 * B; }
	static float FRand() { return 0.5f; }
	static bool IsNearlyEqual(float A, float B, float T = 1e-4f) { return std::fabs(A - B) <= T; }
	static float DegreesToRadians(float D) { return D * 0.0174533f; }
	static float RadiansToDegrees(float R) { return R * 57.2958f; }
	static float Sin(float A) { return std::sin(A); }
	static float Cos(float A) { return std::cos(A); }
	static float Acos(float A) { return std::acos(A); }
	static float Atan2(float A, float B) { return std::atan2(A, B); }
	static int32 RoundToInt(float A) { return (int32)std::lround(A); }
};

// ── 日志 / 控制台 / 统计 ──
struct FLogCategoryBase {};
inline FLogCategoryBase LogTemp;
enum class ELogVerbosity { Display, Warning, Error, Log };
namespace ELogV { constexpr int Display = 0, Warning = 1, Error = 2, Log = 3; }
template <typename... A> inline void FakeLog(const TCHAR*, A&&...) {}
#define UE_LOG(Cat, Verb, Fmt, ...) do { (void)ELogV::Verb; FakeLog(Fmt, ##__VA_ARGS__); } while (0)
struct TStatId {};

class UWorld;
struct FConsoleCommandWithWorldAndArgsDelegate
{
	static FConsoleCommandWithWorldAndArgsDelegate CreateStatic(void (*)(const TArray<FString>&, UWorld*)) { return {}; }
	template <typename L> static FConsoleCommandWithWorldAndArgsDelegate CreateLambda(L) { return {}; }
};
struct FAutoConsoleCommandWithWorldAndArgs
{
	FAutoConsoleCommandWithWorldAndArgs(const TCHAR*, const TCHAR*, const FConsoleCommandWithWorldAndArgsDelegate&) {}
};

// ── 委托 ──
struct FDelegateHandle {};
template <typename... Args>
struct TFakeMulticast
{
	template <typename U> FDelegateHandle AddUObject(U*, void (U::*)(Args...)) { return {}; }
	void Remove(FDelegateHandle) {}
	void Broadcast(Args...) const {}
};
template <typename... Args>
struct TFakeDynMulticast
{
	template <typename U> void AddDynamic(U*, void (U::*)(Args...)) {}
	template <typename U> void AddUniqueDynamic(U*, void (U::*)(Args...)) {}
	template <typename U> void RemoveDynamic(U*, void (U::*)(Args...)) {}
	void Broadcast(Args...) const {}
};
#define DECLARE_MULTICAST_DELEGATE_OneParam(Name, T1) struct Name : TFakeMulticast<T1> {};
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(Name) struct Name : TFakeDynMulticast<> {};
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(Name, T1, P1) struct Name : TFakeDynMulticast<T1> {};
using FSimpleMulticastDelegate = TFakeMulticast<>;

// ── UObject ──
class UClass;
class UWorld;
class UObject
{
public:
	virtual ~UObject() {}
	virtual UWorld* GetWorld() const { return nullptr; }
	FString GetName() const { return FString(); }
	UClass* GetClass() const { return nullptr; }
	bool Modify(bool = true) { return true; }
	virtual void PostInitProperties() {}
	bool TryUpdateDefaultConfigFile(const FString& = FString(), bool = true) { return true; }
	void SaveConfig() {}
};
class UClass : public UObject { public: template <typename T> bool ImplementsInterface(T*) const { return true; } };
struct FPropertyChangedChainEvent { int32 GetArrayIndex(const FString&) const { return -1; } };
class UInterface : public UObject {};

template <typename T> T* NewObject(UObject* Outer = nullptr, const FName& Name = FName()) { (void)Outer; (void)Name; return new T(); }
template <typename T> T* GetMutableDefault() { static T D; return &D; }
template <typename T> const T* GetDefault() { return GetMutableDefault<T>(); }
template <typename To, typename From> To* Cast(From* P) { return dynamic_cast<To*>(P); }
template <typename To, typename From> const To* Cast(const From* P) { return dynamic_cast<const To*>(P); }
inline FString GetNameSafe(const UObject* O) { return O ? O->GetName() : FString(); }

template <typename T>
class TObjectPtr
{
public:
	T* P = nullptr;
	TObjectPtr() {}
	TObjectPtr(std::nullptr_t) {}
	TObjectPtr(T* In) : P(In) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TObjectPtr(U* In) : P(In) {}
	T* Get() const { return P; }
	operator T*() const { return P; }
	T* operator->() const { return P; }
	explicit operator bool() const { return P != nullptr; }
};
template <typename T, typename U> bool operator==(const TObjectPtr<T>& A, U* B) { return A.P == B; }
template <typename T, typename U> bool operator==(U* B, const TObjectPtr<T>& A) { return A.P == B; }
template <typename T> bool operator==(const TObjectPtr<T>& A, std::nullptr_t) { return A.P == nullptr; }
template <typename T, typename U> bool operator==(const TObjectPtr<T>& A, const TObjectPtr<U>& B) { return A.P == B.P; }
template <typename T>
class TWeakObjectPtr
{
public:
	T* P = nullptr;
	TWeakObjectPtr() {}
	TWeakObjectPtr(std::nullptr_t) {}
	TWeakObjectPtr(T* In) : P(In) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>> TWeakObjectPtr(U* In) : P(In) {}
	T* Get() const { return P; }
	bool IsValid() const { return P != nullptr; }
	T* operator->() const { return P; }
	TWeakObjectPtr& operator=(T* In) { P = In; return *this; }
	TWeakObjectPtr& operator=(std::nullptr_t) { P = nullptr; return *this; }
	void Reset() { P = nullptr; }
};
struct FSoftObjectPath
{
	FString Path;
	FSoftObjectPath() {}
	FSoftObjectPath(const TCHAR* In) : Path(In) {}
	bool IsValid() const { return !Path.IsEmpty(); }
	FString GetLongPackageName() const { return Path; }
	FString ToString() const { return Path; }
	bool operator==(const FSoftObjectPath& O) const { return Path == O.Path; }
};
template <typename T>
class TSoftObjectPtr
{
public:
	FSoftObjectPath Path;
	TSoftObjectPtr() {}
	explicit TSoftObjectPtr(const FSoftObjectPath& In) : Path(In) {}
	T* Get() const { return nullptr; }
	T* LoadSynchronous() const { return nullptr; }
	const FSoftObjectPath& ToSoftObjectPath() const { return Path; }
	FString ToString() const { return Path.ToString(); }
};

// ── Tick / 组件 / Actor ──
enum ETickingGroup { TG_PrePhysics, TG_DuringPhysics, TG_PostPhysics };
enum ELevelTick { LEVELTICK_All };
struct FTickFunction { bool bCanEverTick = false; bool bStartWithTickEnabled = true; float TickInterval = 0; ETickingGroup TickGroup = TG_PrePhysics; };
struct FActorTickFunction : FTickFunction {};
struct FActorComponentTickFunction : FTickFunction {};
namespace EEndPlayReason { enum Type { Destroyed, LevelTransition, EndPlayInEditor, RemovedFromWorld, Quit }; }
namespace EWorldType { enum Type { None, Game, Editor, PIE, EditorPreview, GamePreview, Inactive }; }
struct FTimerHandle {};

class AActor;
class UActorComponent : public UObject
{
public:
	FActorComponentTickFunction PrimaryComponentTick;
	TArray<FName> ComponentTags;
	AActor* GetOwner() const { return nullptr; }
	virtual UWorld* GetWorld() const override { return nullptr; }
	void RegisterComponent() {}
	void SetComponentTickEnabled(bool) {}
	virtual void BeginPlay() {}
	virtual void TickComponent(float, ELevelTick, FActorComponentTickFunction*) {}
	template <typename T> T* CreateDefaultSubobject(const TCHAR*) { return new T(); }
};
class USceneComponent : public UActorComponent
{
public:
	void SetWorldLocation(const FVector&) {}
	FVector GetComponentLocation() const { return FVector(); }
	void SetupAttachment(USceneComponent*) {}
};
class UPrimitiveComponent : public USceneComponent {};
class UStaticMesh : public UObject {};
class UStaticMeshComponent : public UPrimitiveComponent { public: UStaticMesh* GetStaticMesh() const { return nullptr; } };
class UCapsuleComponent : public UPrimitiveComponent { public: float GetScaledCapsuleHalfHeight() const { return 90.f; } };
class UMeshComponent : public UPrimitiveComponent {};
class USphereComponent : public UPrimitiveComponent {};
class UPointLightComponent : public USceneComponent {};

struct FHitResult
{
	AActor* GetActor() const { return nullptr; }
	UPrimitiveComponent* GetComponent() const { return nullptr; }
};

class AActor : public UObject
{
public:
	FActorTickFunction PrimaryActorTick;
	TObjectPtr<USceneComponent> RootComponent;
	TArray<FName> Tags;
	virtual UWorld* GetWorld() const override { return nullptr; }
	FVector GetActorLocation() const { return FVector(); }
	FRotator GetActorRotation() const { return FRotator(); }
	FQuat GetActorQuat() const { return FQuat(); }
	FTransform GetActorTransform() const { return FTransform(); }
	const FString& GetActorLabel(bool = true) const { static FString L; return L; }
	bool IsHidden() const { return false; }
	template <typename T> T* FindComponentByClass() const { return nullptr; }
	void AddInstanceComponent(UActorComponent*) {}
	void SetActorTickEnabled(bool) {}
	bool SetActorLocation(const FVector&, bool = false) { return true; }
	bool SetActorRotation(const FRotator&) { return true; }
	bool SetActorRotation(const FQuat&) { return true; }
	void SetActorHiddenInGame(bool) {}
	void SetActorEnableCollision(bool) {}
	USceneComponent* GetRootComponent() const { return RootComponent.Get(); }
	void SetRootComponent(USceneComponent*) {}
	template <typename T> T* CreateDefaultSubobject(const TCHAR*) { return new T(); }
	virtual void BeginPlay() {}
	virtual void EndPlay(const EEndPlayReason::Type) {}
	virtual void Tick(float) {}
};
class UInputComponent : public UActorComponent {};
class AController : public AActor {};
class APawn : public AActor {};
class UCharacterMovementComponent;
struct FFindFloorResult { FHitResult HitResult; };
class UCharacterMovementComponent : public UActorComponent
{
public:
	FFindFloorResult CurrentFloor;
	FVector Velocity;
	bool IsMovingOnGround() const { return true; }
	bool IsFalling() const { return false; }
	FVector GetCurrentAcceleration() const { return FVector(); }
};
class ACharacter : public APawn
{
public:
	int32 JumpCurrentCount = 0;
	UCharacterMovementComponent* GetCharacterMovement() const { return nullptr; }
	UCapsuleComponent* GetCapsuleComponent() const { return nullptr; }
};
class AStaticMeshActor : public AActor {};
class AHUD;
class APlayerController : public AController
{
public:
	APawn* GetPawn() const { return nullptr; }
	AHUD* GetHUD() const { return nullptr; }
	void ConsoleCommand(const FString&) {}
};
class UCanvas : public UObject { public: float SizeX = 0, SizeY = 0; };
class UFont;
class AHUD : public AActor
{
public:
	TObjectPtr<UCanvas> Canvas;
	virtual void DrawHUD() {}
	APlayerController* GetOwningPlayerController() const { return nullptr; }
	void DrawRect(FLinearColor, float, float, float, float) {}
	void DrawText(const FString&, FLinearColor, float, float, UFont* = nullptr, float = 1.f, bool = false) {}
};

class USubsystem : public UObject {};
struct FSubsystemCollectionBase {};
class UGameInstance;
class UGameInstanceSubsystem : public USubsystem { public: virtual void Initialize(FSubsystemCollectionBase&) {} virtual void Deinitialize() {} };
class UWorldSubsystem : public USubsystem
{
public:
	virtual void Initialize(FSubsystemCollectionBase&) {}
	virtual void Deinitialize() {}
	virtual UWorld* GetWorld() const override { return nullptr; }
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type) const { return true; }
};
class UTickableWorldSubsystem : public UWorldSubsystem
{
public:
	virtual void Tick(float) {}
	virtual TStatId GetStatId() const = 0;
};
class UGameInstance : public UObject { public: template <typename T> T* GetSubsystem() const { return nullptr; } };
class UWorld : public UObject
{
public:
	template <typename T> T* GetSubsystem() const { return nullptr; }
	double GetTimeSeconds() const { return 0; }
	float GetDeltaSeconds() const { return 0; }
	APlayerController* GetFirstPlayerController() const { return nullptr; }
	UGameInstance* GetGameInstance() const { return nullptr; }
};
template <typename T>
class TActorIterator
{
public:
	explicit TActorIterator(const UWorld*) {}
	explicit operator bool() const { return false; }
	TActorIterator& operator++() { return *this; }
	T* operator*() const { return nullptr; }
	T* operator->() const { return nullptr; }
};

class UEngine : public UObject { public: void AddOnScreenDebugMessage(int32, float, FColor, const FString&) {} };
inline UEngine* GEngine = nullptr;

class UDeveloperSettings : public UObject
{
public:
	virtual FName GetCategoryName() const { return FName(); }
	virtual FText GetSectionText() const { return FText(); }
	virtual FText GetSectionDescription() const { return FText(); }
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent&) {}
};
class UBlueprintFunctionLibrary : public UObject {};
class USaveGame : public UObject {};

// ── 声音 ──
class USoundBase : public UObject {};
class USoundWave : public USoundBase { public: uint8 bLooping : 1; };
class USoundConcurrency : public UObject {};
namespace EAttenuationShape { enum Type { Sphere, Capsule, Box, Cone }; }
enum class EAttenuationDistanceModel : uint8 { Linear, Logarithmic, Inverse, LogReverse, NaturalSound, Custom };
template <typename E> struct TEnumAsByte { E V; TEnumAsByte& operator=(E In) { V = In; return *this; } };
struct FSoundAttenuationSettings
{
	uint8 bAttenuate : 1;
	uint8 bSpatialize : 1;
	uint8 bAttenuateWithLPF : 1;
	TEnumAsByte<EAttenuationShape::Type> AttenuationShape;
	FVector AttenuationShapeExtents;
	float FalloffDistance = 0;
	EAttenuationDistanceModel DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	float StereoSpread = 0;
	float LPFRadiusMin = 0, LPFRadiusMax = 0, LPFFrequencyAtMin = 0, LPFFrequencyAtMax = 0;
};
class USoundAttenuation : public UObject { public: FSoundAttenuationSettings Attenuation; };
class UAudioComponent : public USceneComponent
{
public:
	void Stop() {}
	bool IsPlaying() const { return false; }
	void SetVolumeMultiplier(float) {}
	void SetPitchMultiplier(float) {}
	void SetLowPassFilterEnabled(bool) {}
	void SetLowPassFilterFrequency(float) {}
};
namespace EAttachLocation { enum Type { KeepRelativeOffset, KeepWorldPosition, SnapToTarget, SnapToTargetIncludingScale }; }
class UGameplayStatics : public UBlueprintFunctionLibrary
{
public:
	static UAudioComponent* SpawnSound2D(const UObject*, USoundBase*, float = 1.f, float = 1.f, float = 0.f, USoundConcurrency* = nullptr, bool = false, bool = true) { return nullptr; }
	static UAudioComponent* SpawnSoundAtLocation(const UObject*, USoundBase*, FVector, FRotator = FRotator::ZeroRotator, float = 1.f, float = 1.f, float = 0.f, USoundAttenuation* = nullptr, USoundConcurrency* = nullptr, bool = true) { return nullptr; }
	static UAudioComponent* SpawnSoundAttached(USoundBase*, USceneComponent*, FName = FName(), FVector = FVector(), EAttachLocation::Type = EAttachLocation::KeepRelativeOffset, bool = false, float = 1.f, float = 1.f, float = 0.f, USoundAttenuation* = nullptr, USoundConcurrency* = nullptr, bool = true) { return nullptr; }
};

// ── 加载 ──
struct FStreamableHandle { void ReleaseHandle() {} };
struct FStreamableManager { TSharedPtr<FStreamableHandle> RequestAsyncLoad(const TArray<FSoftObjectPath>&) { return TSharedPtr<FStreamableHandle>(); } };
struct FPackageName { static bool DoesPackageExist(const FString&, FString* = nullptr, bool = true) { return true; } };

// ── 自动化测试 ──
enum class EAutomationTestFlags : uint32 { EditorContext = 1, ProductFilter = 2, EngineFilter = 4 };
inline EAutomationTestFlags operator|(EAutomationTestFlags A, EAutomationTestFlags B) { return (EAutomationTestFlags)((uint32)A | (uint32)B); }
class FAutomationTestBase
{
public:
	virtual ~FAutomationTestBase() {}
	virtual bool RunTest(const FString&) = 0;
	bool TestTrue(const FString&, bool) { return true; }
	bool TestFalse(const FString&, bool) { return true; }
	template <typename A, typename B> bool TestEqual(const FString&, const A&, const B&) { return true; }
	template <typename T> bool TestNotNull(const FString&, const T*) { return true; }
	void AddError(const FString&) {}
};
#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, Name, Flags) \
	class Class : public FAutomationTestBase { public: virtual bool RunTest(const FString&) override; }; \
	static_assert(sizeof(Name) > 0, ""); static const EAutomationTestFlags Class##_Flags = Flags;

