#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

#include "imgui.h"

#include "imgui_impl_dx9.h"

#include "imgui_impl_win32.h"

#pragma comment(lib, "ws2_32.lib")
#include "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Include/d3dx9.h"
#include <cstdint>

#pragma comment(lib, "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Lib/x86/d3dx9.lib")

__int8 Should_Draw_Graphical_User_Interface;

HWND Overlay_Window;

long long int __stdcall Window_Procedure(HWND Window, unsigned int Message, unsigned long long int Parameter_1, long long int Parameter_2)
{
	if (Message == WM_KEYDOWN)
	{
		if (Parameter_1 == VK_INSERT)
		{
			Should_Draw_Graphical_User_Interface ^= 1;

			if (Should_Draw_Graphical_User_Interface == 1)
			{
				long int Style = GetWindowLongW(Overlay_Window, GWL_EXSTYLE);

				Style &= ~WS_EX_TRANSPARENT;

				SetWindowLongW(Overlay_Window, GWL_EXSTYLE, Style);
			}
			else
			{
				long int Style = GetWindowLongW(Overlay_Window, GWL_EXSTYLE);

				Style |= WS_EX_TRANSPARENT;

				SetWindowLongW(Overlay_Window, GWL_EXSTYLE, Style);
			}
		}
	}
	else
	{
		if (Message == WM_DESTROY)
		{
			PostQuitMessage(EXIT_SUCCESS);
		}
	}

	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	if (ImGui_ImplWin32_WndProcHandler(Window, Message, Parameter_1, Parameter_2) == 1)
	{
		return WM_CREATE;
	}

	return DefWindowProcA(Window, Message, Parameter_1, Parameter_2);
}

constexpr auto packet_magic = 0x54456885;
constexpr auto server_ip = 0x7F000001; // 127.0.0.1
constexpr auto server_port = 69685;

enum class PacketType
{
	packet_copy_memory,
	packet_get_base_address,
	packet_completed
};

struct PacketCopyMemory
{
	uint32_t dest_process_id;
	uint64_t dest_address;

	uint32_t src_process_id;
	uint64_t src_address;

	uint32_t size;
};

struct PacketGetBaseAddress
{
	uint32_t process_id;
};

struct PackedCompleted
{
	uint64_t result;
};

struct PacketHeader
{
	uint32_t   magic;
	PacketType type;
};

struct Packet
{
	PacketHeader header;
	union
	{
		PacketCopyMemory	 copy_memory;
		PacketGetBaseAddress get_base_address;
		PackedCompleted		 completed;
	} data;
};

class Vector3
{
public:
	Vector3() : x(0.f), y(0.f), z(0.f)
	{

	}

	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
	{

	}
	~Vector3()
	{

	}

	float x;
	float y;
	float z;

	float Dot(Vector3 v)
	{
		return x * v.x + y * v.y + z * v.z;
	}

	float Distance(Vector3 v)
	{
		return float(sqrtf(powf(v.x - x, 2.0) + powf(v.y - y, 2.0) + powf(v.z - z, 2.0)));
	}

	Vector3 operator+(Vector3 v)
	{
		return Vector3(x + v.x, y + v.y, z + v.z);
	}

	Vector3 operator-(Vector3 v)
	{
		return Vector3(x - v.x, y - v.y, z - v.z);
	}

	Vector3 operator*(float number) const {
		return Vector3(x * number, y * number, z * number);
	}
};

static bool send_packet(
	const SOCKET	connection,
	const Packet& packet,
	uint64_t& out_result)
{
	Packet completion_packet{ };

	if (send(connection, (const char*)& packet, sizeof(Packet), 0) == SOCKET_ERROR)
		return false;

	const auto result = recv(connection, (char*)& completion_packet, sizeof(Packet), 0);
	if (result < sizeof(PacketHeader) ||
		completion_packet.header.magic != packet_magic ||
		completion_packet.header.type != PacketType::packet_completed)
		return false;

	out_result = completion_packet.data.completed.result;
	return true;
}

static uint32_t copy_memory(
	const SOCKET	connection,
	const uint32_t	src_process_id,
	const uintptr_t src_address,
	const uint32_t	dest_process_id,
	const uintptr_t	dest_address,
	const size_t	size)
{
	Packet packet{ };

	packet.header.magic = packet_magic;
	packet.header.type = PacketType::packet_copy_memory;

	auto& data = packet.data.copy_memory;
	data.src_process_id = src_process_id;
	data.src_address = uint64_t(src_address);
	data.dest_process_id = dest_process_id;
	data.dest_address = uint64_t(dest_address);
	data.size = uint64_t(size);

	uint64_t result = 0;
	if (send_packet(connection, packet, result))
		return uint32_t(result);

	return 0;
}

void initialize()
{
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

void deinitialize()
{
	WSACleanup();
}

SOCKET connect()
{
	SOCKADDR_IN address{ };

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(server_ip);
	address.sin_port = htons(server_port);

	const auto connection = socket(AF_INET, SOCK_STREAM, 0);
	if (connection == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (connect(connection, (SOCKADDR*)& address, sizeof(address)) == SOCKET_ERROR)
	{
		closesocket(connection);
		return INVALID_SOCKET;
	}

	return connection;
}

void disconnect(const SOCKET connection)
{
	closesocket(connection);
}

uint32_t read_memory(
	const SOCKET	connection,
	const uint32_t	process_id,
	const uintptr_t address,
	const uintptr_t buffer,
	const size_t	size)
{
	return copy_memory(connection, process_id, address, GetCurrentProcessId(), buffer, size);
}

uint32_t write_memory(
	const SOCKET	connection,
	const uint32_t	process_id,
	const uintptr_t address,
	const uintptr_t buffer,
	const size_t	size)
{
	return copy_memory(connection, GetCurrentProcessId(), buffer, process_id, address, size);
}

uint64_t get_process_base_address(const SOCKET connection, const uint32_t process_id)
{
	Packet packet{ };

	packet.header.magic = packet_magic;
	packet.header.type = PacketType::packet_get_base_address;

	auto& data = packet.data.get_base_address;
	data.process_id = process_id;

	uint64_t result = 0;
	if (send_packet(connection, packet, result))
		return result;

	return 0;
}

template <typename T>
T read(const SOCKET connection, const uint32_t process_id, const uintptr_t address)
{
	T buffer{ };
	read_memory(connection, process_id, address, uint64_t(&buffer), sizeof(T));

	return buffer;
}

template <typename T>
void write(const SOCKET connection, const uint32_t process_id, const uintptr_t address, const T& buffer)
{
	write_memory(connection, process_id, address, uint64_t(&buffer), sizeof(T));
}
//update thread
#define M_PI 3.14159265358979323846264338327950288419716939937510
#define OFFSET_UWORLD 0x65A4AF0 //48 8b 0d ? ? ? ? 48 85 c9 74 30 e8 ? ? ? ? 48 8b f8
#define location_Offs 0x6583790 //F3 44 0F 11 1D ? ? ? ?
DWORD processID;
HWND hwnd = NULL;

int width;
int height;
int localplayerID;

UINT_PTR connection;
uint64_t base_address;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
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
	DWORD_PTR bonearray = read<DWORD_PTR>(connection, processID, mesh + 0x3F8);

	return read<FTransform>(connection, processID, bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(connection, processID, mesh + 0x1C0);
	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

Vector3 Camera(unsigned __int64 Pawn, unsigned __int64 RootComponent)
{
	unsigned __int64 PtrPitch;
	Vector3 Camera;

	PtrPitch = read<unsigned __int64>(connection, processID, Pawn + 0x50);
	PtrPitch = read<unsigned __int64>(connection, processID, PtrPitch + 0x848);

	Camera.x = read<float>(connection, processID, RootComponent + 0x12C); // YAW
	Camera.y = read<float>(connection, processID, PtrPitch + 0x698);      // PITCH
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

Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Rotation = camrot; // FRotator

	D3DMATRIX tempMatrix = Matrix(Rotation);

	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 camloc = read<Vector3>(connection, processID, base_address + location_Offs);

	Vector3 vDelta = WorldLocation - camloc;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float FovAngle = 80.0f;
	float ScreenCenterX = width / 2.0f; //fortnite window size here
	float ScreenCenterY = height / 2.0f; //fortnite window size here

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}


void updateaddr()
{
	Uworld = read<DWORD_PTR>(connection, processID, base_address + OFFSET_UWORLD);
	//printf(_xor_("Uworld: %p.\n").c_str(), Uworld);

	DWORD_PTR Gameinstance = read<DWORD_PTR>(connection, processID, Uworld + 0x160);
	//printf(_xor_("Gameinstance: %p.\n").c_str(), Gameinstance);

	DWORD_PTR LocalPlayers = read<DWORD_PTR>(connection, processID, Gameinstance + 0x38);
	//printf(_xor_("LocalPlayers: %p.\n").c_str(), LocalPlayers);

	Localplayer = read<DWORD_PTR>(connection, processID, LocalPlayers);
	//printf(_xor_("LocalPlayer: %p.\n").c_str(), Localplayer);

	PlayerController = read<DWORD_PTR>(connection, processID, Localplayer + 0x30);
	//printf(_xor_("playercontroller: %p.\n").c_str(), PlayerController);

	LocalPawn = read<uint64_t>(connection, processID, PlayerController + 0x298);
	//printf(_xor_("Pawn: %p.\n").c_str(), Pawn);

	Rootcomp = read<uint64_t>(connection, processID, LocalPawn + 0x130);
	//printf(_xor_("Rootcomp: %p.\n").c_str(), Rootcomp);
}

int __stdcall wWinMain(HINSTANCE This_Module, HINSTANCE Previous_This_Module, wchar_t* Command_Line_Arguments, int Show_Type)
{
	Should_Draw_Graphical_User_Interface = 0;

	WNDCLASSEXA Window_Class_Extended =
	{
		sizeof WNDCLASSEXA,

		CS_VREDRAW | CS_HREDRAW,

		(WNDPROC)Window_Procedure,

		0,

		0,

		GetModuleHandleW(nullptr),

		nullptr,

		nullptr,

		nullptr,

		nullptr,

		"Fortnite_Overlay",

		nullptr
	};

	RegisterClassExA(&Window_Class_Extended);

	RECT Desktop_Rectangle;

	GetWindowRect(GetDesktopWindow(), &Desktop_Rectangle);

	long int Screen_Width = Desktop_Rectangle.right;

	long int Screen_Height = Desktop_Rectangle.bottom;

	Overlay_Window = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, Window_Class_Extended.lpszClassName, "", WS_POPUP, 0, 0, Screen_Width, Screen_Height, nullptr, nullptr, Window_Class_Extended.hInstance, nullptr);

	SetLayeredWindowAttributes(Overlay_Window, 0, 255, LWA_ALPHA);

	MARGINS Margins =
	{
		0,

		0,

		Screen_Width,

		Screen_Height
	};

	DwmExtendFrameIntoClientArea(Overlay_Window, &Margins);

	IDirect3D9* Direct_3_Dimensional_9 = Direct3DCreate9(D3D_SDK_VERSION);

	D3DPRESENT_PARAMETERS Direct_3_Dimensional_Present_Parameters;

	memset(&Direct_3_Dimensional_Present_Parameters, 0, sizeof Direct_3_Dimensional_Present_Parameters);

	Direct_3_Dimensional_Present_Parameters.BackBufferFormat = D3DFMT_A8R8G8B8;

	Direct_3_Dimensional_Present_Parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;

	Direct_3_Dimensional_Present_Parameters.Windowed = 1;

	Direct_3_Dimensional_Present_Parameters.EnableAutoDepthStencil = 1;

	Direct_3_Dimensional_Present_Parameters.AutoDepthStencilFormat = D3DFMT_D16;

	Direct_3_Dimensional_Present_Parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	IDirect3DDevice9* Direct_3_Dimensional_Device_9;

	Direct_3_Dimensional_9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Overlay_Window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &Direct_3_Dimensional_Present_Parameters, &Direct_3_Dimensional_Device_9);

	ShowWindow(Overlay_Window, Show_Type);

	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGui_ImplDX9_Init(Direct_3_Dimensional_Device_9);

	ImGui_ImplWin32_Init(Overlay_Window);

	initialize();

	connection = connect();

	GetWindowThreadProcessId(FindWindowA(nullptr, "Fortnite  "), &processID); 

	base_address = get_process_base_address(connection, processID);

	width = Screen_Width;

	height = Screen_Height;

	MSG Message;

	memset(&Message, 0, sizeof Message);

Label:
	{
		if (Message.message != WM_QUIT)
		{
			if (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE) == 0)
			{
				ImGui_ImplDX9_NewFrame();

				ImGui_ImplWin32_NewFrame();

				ImGui::NewFrame();

				static __int8 d = 0;
				static __int8 aim = 1;
				static __int8 desp = 0;
				static __int8 box = 0;
				ImGuiStyle* Graphical_User_Interface_Style = &ImGui::GetStyle();

				Graphical_User_Interface_Style->WindowPadding = ImVec2(0, 0);

				Graphical_User_Interface_Style->WindowRounding = 0;

				Graphical_User_Interface_Style->WindowBorderSize = 0;

				Graphical_User_Interface_Style->Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);

				ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
				{
					ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Once);

					ImGui::SetWindowSize(ImVec2((float)Screen_Width, (float)Screen_Height), ImGuiCond_Always);


					updateaddr();

					if (LocalPawn != 0) {
						localplayerID = read<int>(connection, processID, LocalPawn + 0x18);
						//std::cout << _xor_("localplayerID = ").c_str() << localplayerID << std::endl;
					}

					Ulevel = read<DWORD_PTR>(connection, processID, Uworld + 0x30);
					//printf(_xor_("Ulevel: %p.\n").c_str(), Ulevel);

					DWORD ActorCount = read<DWORD>(connection, processID, Ulevel + 0xA0);
					//printf(_xor_("ActorCount: %p.\n").c_str(), ActorCount);

					DWORD_PTR AActors = read<DWORD_PTR>(connection, processID, Ulevel + 0x98);
					//printf(_xor_("AActors: %p.\n").c_str(), AActors);

					for (int i = 0; i < ActorCount; i++)
					{
						uint64_t CurrentActor = read<uint64_t>(connection, processID, AActors + i * 0x8);

						if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
							continue;

						int curactorid = read<int>(connection, processID, CurrentActor + 0x18);
						//std::cout << _xor_("curactorid = ").c_str() << curactorid << std::endl;

						if (curactorid != localplayerID)
							continue;

						uint64_t CurrentActorRootComponent = read<uint64_t>(connection, processID, CurrentActor + 0x130);

						if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
							continue;

						uint64_t currentactormesh = read<uint64_t>(connection, processID, CurrentActor + 0x278);

						if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
							continue;

						Vector3 CurrentActorPosition = GetBoneWithRotation(currentactormesh, 66);//read<Vector3>(connection, processID, CurrentActorRootComponent + 0x11C);
						Vector3 Localcam = Camera(LocalPawn, Rootcomp);
						Vector3 localactorpos = read<Vector3>(connection, processID, Rootcomp + 0x11C);



						Vector3 Localpos = read<Vector3>(connection, processID, Rootcomp + 0x11C);
						float distance = Localpos.Distance(CurrentActorPosition) / 100.f;

						//W2S
						Vector3 screenlocation = ProjectWorldToScreen(CurrentActorPosition, Vector3(Localcam.y, Localcam.x, Localcam.z));
						if (d == 1)
						{
							ImGui::SetCursorScreenPos(ImVec2(screenlocation.x, screenlocation.y));
							ImGui::Text("Enemy");
							//DrawBox(screenlocation.x, screenlocation.y, 45.f, 85.f, 2.5f, 255, 0, 0, 255, false); //box esp kek
						}
						if (aim == 1)
						{
							float headX = screenlocation.x - GetSystemMetrics(SM_CXSCREEN) / 2;
							float headY = screenlocation.y - GetSystemMetrics(SM_CYSCREEN) / 2;

							if (headX >= -100 && headX <= 100 && headY >= -100 && headY <= 100) {
								if (GetAsyncKeyState(VK_RBUTTON) && headX != 0 && headY != 0) {
									mouse_event(MOUSEEVENTF_MOVE, headX, headY, NULL, NULL);
								}
							}
						}
						if (desp == 1)
						{
							ImGui::SetCursorScreenPos(ImVec2(screenlocation.x, screenlocation.y + 20));
							ImGui::Text("[ %f ]", distance);
						}
						if (box == 1)
						{
							Vector3 Right_Foot = GetBoneWithRotation(currentactormesh, 76);

							Vector3 Top = ProjectWorldToScreen(Right_Foot, Vector3(Localcam.y, Localcam.x, Localcam.z - 72));


							Vector3 Bottom = ProjectWorldToScreen(Right_Foot, Vector3(Localcam.y, Localcam.x, Localcam.z + 72));

							float Height = Bottom.y - Top.y;

							float Width = Height / 2;

							float X = Top.x - Width / 2;

							float Y = Top.y;

							ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(X, Y), ImVec2(X + Width, Y + Height), IM_COL32(128, 255, 0, 0), 0, ImDrawCornerFlags_None);

						}
					}
				
			
					
					ImGui::SetCursorScreenPos(ImVec2(0, 0));

					ImGui::Text("Overlay FPS: %f", ImGui::GetIO().Framerate);
					ImGui::End();
				}

				if (Should_Draw_Graphical_User_Interface == 1)
				{
					Graphical_User_Interface_Style->WindowPadding = ImVec2(8, 8);

					Graphical_User_Interface_Style->WindowRounding = 7;

					Graphical_User_Interface_Style->WindowBorderSize = 1;

					Graphical_User_Interface_Style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);

					ImGui::Begin("Fortnite", nullptr, ImGuiWindowFlags_NoCollapse );
					{
						static short tab = 0;
						if (ImGui::Button("Visual", ImVec2(50, 20)))
							tab = 0;
						ImGui::SameLine();
						if (ImGui::Button("Aimbot", ImVec2(50, 20)))
							tab = 1;


						if (tab == 0) {
							ImGui::Checkbox("Enable", (bool*)& d);
							ImGui::Checkbox("Distance ESP", (bool*)& desp);
							ImGui::Checkbox("Box ESP", (bool*)& box);

						}
						if (tab == 1) {
							ImGui::Checkbox("Aimbot", (bool*)& aim);
						}

						ImGui::End();
					}

					ImGui::EndFrame();
				}

				Direct_3_Dimensional_Device_9->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1, 0);

				if (Direct_3_Dimensional_Device_9->BeginScene() == D3D_OK)
				{
					ImGui::Render();

					ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

					Direct_3_Dimensional_Device_9->EndScene();
				}

				if (Direct_3_Dimensional_Device_9->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST)
				{
					if (Direct_3_Dimensional_Device_9->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
					{
						ImGui_ImplDX9_InvalidateDeviceObjects();

						Direct_3_Dimensional_Device_9->Reset(&Direct_3_Dimensional_Present_Parameters);

						ImGui_ImplDX9_CreateDeviceObjects();
					}
				}
			}
			else
			{
				TranslateMessage(&Message);

				DispatchMessageA(&Message);
			}

			goto Label;
		}
	}

	return EXIT_SUCCESS;
}