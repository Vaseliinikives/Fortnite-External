#include "DirectOverlay.h"

#define M_PI 180.0 / 3.14159265358979323846//3.14159265358979323846264338327950288419716939937510
#define OFFSET_UWORLD 0x065a4af0

DWORD processID;
HWND hwnd = NULL;

int width;
int height;
int localplayerID;

UINT_PTR connection;
uint64_t base_address;

DWORD_PTR Uworld;
DWORD_PTR Pawn;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Ulevel;


struct FQuat
{
	float x;
	float y;
	float z;
	float w;
};

struct FTransform
{
	FQuat rot;
	Vector3 translation;
	char pad[4];
	Vector3 scale;
	char pad1[4];
	D3DMATRIX ToMatrixWithScale()
	{
		D3DMATRIX m;
		m._41 = translation.x;
		m._42 = translation.y;
		m._43 = translation.z;

		float x2 = rot.x + rot.x;
		float y2 = rot.y + rot.y;
		float z2 = rot.z + rot.z;

		float xx2 = rot.x * x2;
		float yy2 = rot.y * y2;
		float zz2 = rot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * scale.x;
		m._22 = (1.0f - (xx2 + zz2)) * scale.y;
		m._33 = (1.0f - (xx2 + yy2)) * scale.z;

		float yz2 = rot.y * z2;
		float wx2 = rot.w * x2;
		m._32 = (yz2 - wx2) * scale.z;
		m._23 = (yz2 + wx2) * scale.y;

		float xy2 = rot.x * y2;
		float wz2 = rot.w * z2;
		m._21 = (xy2 - wz2) * scale.y;
		m._12 = (xy2 + wz2) * scale.x;

		float xz2 = rot.x * z2;
		float wy2 = rot.w * y2;
		m._31 = (xz2 + wy2) * scale.z;
		m._13 = (xz2 - wy2) * scale.x;

		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;

		return m;
	}
};

D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
{
	D3DMATRIX pOut;
	pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
	pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
	pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
	pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
	pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
	pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
	pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
	pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
	pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
	pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
	pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
	pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
	pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
	pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
	pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
	pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;

	return pOut;
}

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray = driver::read<DWORD_PTR>(connection, processID, mesh + 0x3F8);

	return driver::read<FTransform>(connection, processID, bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = driver::read<FTransform>(connection, processID, mesh + 0x1C0);
	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

Vector3 Camera(unsigned __int64 Pawn, unsigned __int64 RootComponent)
{
	unsigned __int64 PtrPitch; 
	Vector3 Camera;

	PtrPitch = driver::read<unsigned __int64>(connection, processID, Pawn + 0x50);
	PtrPitch = driver::read<unsigned __int64>(connection, processID, PtrPitch + 0x848);

	Camera.x = driver::read<float>(connection, processID, RootComponent + 0x12C); // YAW
	Camera.y = driver::read<float>(connection, processID, PtrPitch + 0x698);      // PITCH
	Camera.z = 0.f;																  // ROLL

	float test = asin(Camera.y);
	float degrees = test * (180.0 / M_PI);

	Camera.y = degrees;

	if (Camera.x < 0)
		Camera.x = 360 + Camera.x;

	return Camera;
}

D3DXMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot, Vector3 camloc)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Rotation = camrot; // FRotator

	D3DMATRIX tempMatrix = Matrix(Rotation);

	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 vDelta = WorldLocation - camloc;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float FovAngle = 80.f;
	float ScreenCenterX = width / 2.0f;  //fortnite window x size here
	float ScreenCenterY = height / 2.0f; //fortnite window y size here

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
    Screenlocation.z = 0;

	return Screenlocation;
}


void updateaddr()
{
	Uworld = driver::read<DWORD_PTR>(connection, processID, base_address + OFFSET_UWORLD);
	//printf(_xor_("Uworld: %p.\n").c_str(), Uworld);

	DWORD_PTR Gameinstance = driver::read<DWORD_PTR>(connection, processID, Uworld + 0x160);
//	printf(_xor_("Gameinstance: %p.\n").c_str(), Gameinstance);

	DWORD_PTR LocalPlayers = driver::read<DWORD_PTR>(connection, processID, Gameinstance + 0x38);
	//printf(_xor_("LocalPlayers: %p.\n").c_str(), LocalPlayers);

	Localplayer = driver::read<DWORD_PTR>(connection, processID, LocalPlayers);
	//printf(_xor_("LocalPlayer: %p.\n").c_str(), Localplayer);

	PlayerController = driver::read<DWORD_PTR>(connection, processID, Localplayer + 0x30);
	//printf(_xor_("playercontroller: %p.\n").c_str(), PlayerController);

	Pawn = driver::read<uint64_t>(connection, processID, PlayerController + 0x298);
	//printf(_xor_("Pawn: %p.\n").c_str(), Pawn);

	Rootcomp = driver::read<uint64_t>(connection, processID, Pawn + 0x130);
	//printf(_xor_("Rootcomp: %p.\n").c_str(), Rootcomp);
}

void drawLoop(int width, int height) {
	updateaddr();

	if (Pawn != 0) {
		localplayerID = driver::read<int>(connection, processID, Pawn + 0x18);
		//std::cout << _xor_("localplayerID = ").c_str() << localplayerID << std::endl;
	}

	Ulevel = driver::read<DWORD_PTR>(connection, processID, Uworld + 0x30);
	//printf(_xor_("Ulevel: %p.\n").c_str(), Ulevel);

	DWORD ActorCount = driver::read<DWORD>(connection, processID, Ulevel + 0xA0);
	//printf(_xor_("ActorCount: %p.\n").c_str(), ActorCount);

	DWORD_PTR AActors = driver::read<DWORD_PTR>(connection, processID, Ulevel + 0x98);
	//printf(_xor_("AActors: %p.\n").c_str(), AActors);

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t CurrentActor = driver::read<uint64_t>(connection, processID, AActors + i * 0x8);

		if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
			continue;

		int curactorid = driver::read<int>(connection, processID, CurrentActor + 0x18);
		//std::cout << _xor_("curactorid = ").c_str() << curactorid << std::endl;

		if (curactorid != localplayerID)
			continue;

		uint64_t CurrentActorRootComponent = driver::read<uint64_t>(connection, processID, CurrentActor + 0x130);

		if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
			continue;

		uint64_t currentactormesh = driver::read<uint64_t>(connection, processID, CurrentActor + 0x278);

		if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
			continue;

		Vector3 CurrentActorPosition = GetBoneWithRotation(currentactormesh, 66);//driver::read<Vector3>(connection, processID, CurrentActorRootComponent + 0x11C);
		Vector3 Localcam = Camera(Pawn, Rootcomp);
		Vector3 localactorpos = driver::read<Vector3>(connection, processID, Rootcomp + 0x11C);

		//W2S
		Vector3 screenlocation = ProjectWorldToScreen(CurrentActorPosition, Vector3(Localcam.y, Localcam.x, Localcam.z), localactorpos);

		DrawString(_xor_("nigger").c_str(), 15, screenlocation.x, screenlocation.y, 0, 255, 0, 255); //text esp;D
		//DrawBox(screenlocation.x, screenlocation.y, 45.f, 85.f, 2.5f, 255, 0, 0, 255, false); //box esp kek
		 
	}
	Sleep(1);
}

void main()
{
	printf(_xor_("Initializing driver...\n").c_str());
	driver::initialize();

	while (hwnd == NULL)
	{
		hwnd = FindWindowA(0, _xor_("Fortnite  ").c_str());

		printf(_xor_("Looking for process...\n").c_str());
		Sleep(10);
	}

	GetWindowThreadProcessId(hwnd, &processID);
	//printf(_xor_("pid: %p.\n").c_str(), processID);

	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	connection = driver::connect();

	if (connection == INVALID_SOCKET)
	{
		system(_xor_("PAUSE").c_str());
	}

	base_address = driver::get_process_base_address(connection, processID);
	std::printf(_xor_("Process base address: %p.\n").c_str(), (void*)base_address);

	DirectOverlaySetOption(D2DOV_DRAW_FPS | D2DOV_FONT_ARIAL);
	DirectOverlaySetup(drawLoop, FindWindow(NULL, _xor_("Fortnite  ").c_str()));
	getchar();
}
