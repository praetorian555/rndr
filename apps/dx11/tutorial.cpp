#include <Windows.h>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <iostream>
#include <string>

template <typename T>
inline void SafeRelease(T& Ptr)
{
    if (Ptr)
    {
        Ptr->Release();
        Ptr = nullptr;
    }
}

const LONG g_WindowWidth = 1280;
const LONG g_WindowHeight = 720;
LPCSTR g_WindowClassName = "DirectXWindowClass";
LPCSTR g_WindowName = "DirectX Template";
HWND g_WindowHandle = 0;
const BOOL g_EnableVSync = TRUE;

// Direct3D device and swap chain.
ID3D11Device* g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dDeviceContext = nullptr;
IDXGISwapChain* g_d3dSwapChain = nullptr;

// Render target view for the back buffer of the swap chain.
ID3D11RenderTargetView* g_d3dRenderTargetView = nullptr;

// Depth/stencil view for use as a depth buffer.
ID3D11DepthStencilView* g_d3dDepthStencilView = nullptr;

// A texture to associate to the depth stencil view.
ID3D11Texture2D* g_d3dDepthStencilBuffer = nullptr;

// Define the functionality of the depth/stencil stages.
ID3D11DepthStencilState* g_d3dDepthStencilState = nullptr;

// Define the functionality of the rasterizer stage.
ID3D11RasterizerState* g_d3dRasterizerState = nullptr;
D3D11_VIEWPORT g_Viewport = {0};

// Vertex buffer data
ID3D11InputLayout* g_d3dInputLayout = nullptr;
ID3D11Buffer* g_d3dVertexBuffer = nullptr;
ID3D11Buffer* g_d3dIndexBuffer = nullptr;

// Shader data
ID3D11VertexShader* g_d3dVertexShader = nullptr;
ID3D11PixelShader* g_d3dPixelShader = nullptr;

// Shader resources
enum ConstantBuffer
{
    CB_Application,
    CB_Frame,
    CB_Object,
    NumConstantBuffers
};
ID3D11Buffer* g_d3dConstantBuffers[NumConstantBuffers];

// Demo parameters
DirectX::XMMATRIX g_WorldMatrix;
DirectX::XMMATRIX g_ViewMatrix;
DirectX::XMMATRIX g_ProjectionMatrix;

// Vertex data for a colored cube.
struct VertexPosColor
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Color;
};

VertexPosColor g_Vertices[8] = {

    {DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)},  // 0
    {DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)},   // 1
    {DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f)},    // 2
    {DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f)},   // 3
    {DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f)},   // 4
    {DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f)},    // 5
    {DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)},     // 6
    {DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f)}     // 7
};

WORD g_Indicies[36] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7};

// Forward declarations.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int InitDirectX(HINSTANCE Instance, BOOL bVerticalSync);

// Get the latest profile for the specified shader type.
template <class ShaderClass>
std::string GetLatestProfile();
template <class ShaderClass>
ShaderClass* CreateShader(ID3DBlob* pShaderBlob, ID3D11ClassLinkage* pClassLinkage);
template <class ShaderClass>
ShaderClass* LoadShader(const std::wstring& fileName, const std::string& entryPoint, const std::string& profile);

int InitApplication(HINSTANCE Instance, int CmdShow);
int Run();
void Present(bool vSync);

bool LoadContent();
void UnloadContent();

void Update(float deltaTime);
void Render();
void Cleanup();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow)
{
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);

    // Check for DirectX Math library support.
    if (!DirectX::XMVerifyCPUSupport())
    {
        MessageBox(nullptr, TEXT("Failed to verify DirectX Math library support."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (InitApplication(hInstance, cmdShow) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create applicaiton window."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (InitDirectX(hInstance, g_EnableVSync) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create DirectX device and swap chain."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (!LoadContent())
    {
        MessageBox(nullptr, TEXT("Failed to load content."), TEXT("Error"), MB_OK);
        return -1;
    }

    int ReturnCode = Run();

    UnloadContent();
    Cleanup();

    return ReturnCode;
}

/**
 * Initialize the application window.
 */
int InitApplication(HINSTANCE Instance, int CmdShow)
{
    WNDCLASSEX wndClass = {0};
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = Instance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = g_WindowClassName;

    if (!RegisterClassEx(&wndClass))
    {
        return -1;
    }

    RECT WindowRect = {0, 0, g_WindowWidth, g_WindowHeight};
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

    g_WindowHandle =
        CreateWindowA(g_WindowClassName, g_WindowName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                      WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, nullptr, nullptr, Instance, nullptr);
    if (!g_WindowHandle)
    {
        return -1;
    }

    ShowWindow(g_WindowHandle, CmdShow);
    UpdateWindow(g_WindowHandle);

    return 0;
}

LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT PaintStruct;
    HDC hDC;

    switch (Message)
    {
        case WM_PAINT:
        {
            hDC = BeginPaint(Window, &PaintStruct);
            EndPaint(Window, &PaintStruct);
        }
        break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
        break;
        default:
        {
            return DefWindowProc(Window, Message, wParam, lParam);
        }
    }

    return 0;
}

/**
 * The main application loop.
 */
int Run()
{
    static DWORD PreviousTime = timeGetTime();
    static const float TargetFramerate = 30.0f;
    static const float MaxTimeStep = 1.0f / TargetFramerate;

    MSG Msg = {0};
    while (Msg.message != WM_QUIT)
    {
        if (PeekMessage(&Msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
        else
        {
            const DWORD CurrentTime = timeGetTime();
            float DeltaTime = (CurrentTime - PreviousTime) / 1000.0f;
            PreviousTime = CurrentTime;
            // Cap the delta time to the max time step (useful if your
            // debugging and you don't want the deltaTime value to explode.
            DeltaTime = std::min<float>(DeltaTime, MaxTimeStep);

            Update(DeltaTime);
            Render();

            Present(g_EnableVSync);
        }
    }

    return static_cast<int>(Msg.wParam);
}

/**
 * Initialize the DirectX device and swap chain.
 */
int InitDirectX(HINSTANCE Instance, BOOL bVerticalSync)
{
    // A window handle must have been created already.
    assert(g_WindowHandle != 0);

    RECT ClientRect;
    GetClientRect(g_WindowHandle, &ClientRect);
    // Compute the exact client dimensions. This will be used to initialize the render targets for our swap chain.
    unsigned int ClientWidth = ClientRect.right - ClientRect.left;
    unsigned int ClientHeight = ClientRect.bottom - ClientRect.top;

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.BufferDesc.Width = ClientWidth;
    SwapChainDesc.BufferDesc.Height = ClientHeight;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferDesc.RefreshRate = DXGI_RATIONAL{0, 1};  // QueryRefreshRate(ClientWidth, ClientHeight, bVerticalSync);
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = g_WindowHandle;
    // If you want no multisamling use count=1 and quality=0
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.Windowed = TRUE;
    UINT CreateDeviceFlags = 0;
#if _DEBUG
    CreateDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                         D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};
    // This will be the feature level that is used to create our device and swap chain.
    D3D_FEATURE_LEVEL FeatureLevel;
    IDXGIAdapter* Adapter = nullptr;  // Use default adapter
    HMODULE SoftwareRasterizerModule = nullptr;
    HRESULT Result = D3D11CreateDeviceAndSwapChain(Adapter, D3D_DRIVER_TYPE_HARDWARE, SoftwareRasterizerModule, CreateDeviceFlags,
                                                   FeatureLevels, _countof(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc,
                                                   &g_d3dSwapChain, &g_d3dDevice, &FeatureLevel, &g_d3dDeviceContext);
    if (FAILED(Result))
    {
        return -1;
    }

    // Next initialize the back buffer of the swap chain and associate it to a render target view.
    ID3D11Texture2D* BackBuffer;
    Result = g_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuffer);
    if (FAILED(Result))
    {
        return -1;
    }
    Result = g_d3dDevice->CreateRenderTargetView(BackBuffer, nullptr, &g_d3dRenderTargetView);
    if (FAILED(Result))
    {
        return -1;
    }
    SafeRelease(BackBuffer);

    // Create the depth buffer for use with the depth/stencil view.
    D3D11_TEXTURE2D_DESC DepthStencilBufferDesc;
    ZeroMemory(&DepthStencilBufferDesc, sizeof(D3D11_TEXTURE2D_DESC));
    DepthStencilBufferDesc.ArraySize = 1;
    DepthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    DepthStencilBufferDesc.CPUAccessFlags = 0;  // No CPU access required.
    DepthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilBufferDesc.Width = ClientWidth;
    DepthStencilBufferDesc.Height = ClientHeight;
    DepthStencilBufferDesc.MipLevels = 1;
    DepthStencilBufferDesc.SampleDesc.Count = 1;
    DepthStencilBufferDesc.SampleDesc.Quality = 0;
    DepthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    Result = g_d3dDevice->CreateTexture2D(&DepthStencilBufferDesc, nullptr, &g_d3dDepthStencilBuffer);
    if (FAILED(Result))
    {
        return -1;
    }
    Result = g_d3dDevice->CreateDepthStencilView(g_d3dDepthStencilBuffer, nullptr, &g_d3dDepthStencilView);
    if (FAILED(Result))
    {
        return -1;
    }

    // Setup depth/stencil state.
    D3D11_DEPTH_STENCIL_DESC DepthStencilStateDesc;
    ZeroMemory(&DepthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
    DepthStencilStateDesc.DepthEnable = TRUE;
    DepthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DepthStencilStateDesc.StencilEnable = FALSE;
    Result = g_d3dDevice->CreateDepthStencilState(&DepthStencilStateDesc, &g_d3dDepthStencilState);
    if (FAILED(Result))
    {
        return -1;
    }

    // Setup rasterizer state.
    D3D11_RASTERIZER_DESC RasterizerDesc;
    ZeroMemory(&RasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
    RasterizerDesc.AntialiasedLineEnable = FALSE;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.DepthBias = 0;
    RasterizerDesc.DepthBiasClamp = 0.0f;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.FrontCounterClockwise = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.SlopeScaledDepthBias = 0.0f;
    Result = g_d3dDevice->CreateRasterizerState(&RasterizerDesc, &g_d3dRasterizerState);
    if (FAILED(Result))
    {
        return -1;
    }

    // Initialize the viewport to occupy the entire client area.
    g_Viewport.Width = static_cast<float>(ClientWidth);
    g_Viewport.Height = static_cast<float>(ClientHeight);
    g_Viewport.TopLeftX = 0.0f;
    g_Viewport.TopLeftY = 0.0f;
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;

    return 0;
}

template <>
std::string GetLatestProfile<ID3D11VertexShader>()
{
    assert(g_d3dDevice);

    // Query the current feature level:
    D3D_FEATURE_LEVEL FeatureLevel = g_d3dDevice->GetFeatureLevel();

    switch (FeatureLevel)
    {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
        {
            return "vs_5_0";
        }
        break;
        case D3D_FEATURE_LEVEL_10_1:
        {
            return "vs_4_1";
        }
        break;
        case D3D_FEATURE_LEVEL_10_0:
        {
            return "vs_4_0";
        }
        break;
        case D3D_FEATURE_LEVEL_9_3:
        {
            return "vs_4_0_level_9_3";
        }
        break;
        case D3D_FEATURE_LEVEL_9_2:
        case D3D_FEATURE_LEVEL_9_1:
        {
            return "vs_4_0_level_9_1";
        }
        break;
    }

    return "";
}

template <>
std::string GetLatestProfile<ID3D11PixelShader>()
{
    assert(g_d3dDevice);

    // Query the current feature level:
    D3D_FEATURE_LEVEL FeatureLevel = g_d3dDevice->GetFeatureLevel();
    switch (FeatureLevel)
    {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
        {
            return "ps_5_0";
        }
        break;
        case D3D_FEATURE_LEVEL_10_1:
        {
            return "ps_4_1";
        }
        break;
        case D3D_FEATURE_LEVEL_10_0:
        {
            return "ps_4_0";
        }
        break;
        case D3D_FEATURE_LEVEL_9_3:
        {
            return "ps_4_0_level_9_3";
        }
        break;
        case D3D_FEATURE_LEVEL_9_2:
        case D3D_FEATURE_LEVEL_9_1:
        {
            return "ps_4_0_level_9_1";
        }
        break;
    }
    return "";
}

template <>
ID3D11VertexShader* CreateShader<ID3D11VertexShader>(ID3DBlob* ShaderBlob, ID3D11ClassLinkage* ClassLinkage)
{
    assert(g_d3dDevice);
    assert(ShaderBlob);

    HRESULT Result;

    ID3D11VertexShader* VertexShader = nullptr;
    g_d3dDevice->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), ClassLinkage, &VertexShader);

    // Create the input layout for the vertex shader.
    D3D11_INPUT_ELEMENT_DESC VertexLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(VertexPosColor, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(VertexPosColor, Color), D3D11_INPUT_PER_VERTEX_DATA, 0}};
    Result = g_d3dDevice->CreateInputLayout(VertexLayoutDesc, _countof(VertexLayoutDesc), ShaderBlob->GetBufferPointer(),
                                            ShaderBlob->GetBufferSize(), &g_d3dInputLayout);
    if (FAILED(Result))
    {
        return nullptr;
    }

    return VertexShader;
}

template <>
ID3D11PixelShader* CreateShader<ID3D11PixelShader>(ID3DBlob* ShaderBlob, ID3D11ClassLinkage* ClassLinkage)
{
    assert(g_d3dDevice);
    assert(ShaderBlob);

    ID3D11PixelShader* PixelShader = nullptr;
    g_d3dDevice->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), ClassLinkage, &PixelShader);

    return PixelShader;
}

template <class ShaderClass>
ShaderClass* LoadShader(const std::wstring& FileName, const std::string& EntryPoint, const std::string& _Profile)
{
    ID3DBlob* ShaderBlob = nullptr;
    ID3DBlob* ErrorBlob = nullptr;
    ShaderClass* Shader = nullptr;

    std::string Profile = _Profile;
    if (Profile == "latest")
    {
        Profile = GetLatestProfile<ShaderClass>();
    }

    UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    Flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT Result;
    const D3D_SHADER_MACRO* MacroDefinitions = nullptr;
    Result = D3DCompileFromFile(FileName.c_str(), MacroDefinitions, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(), Profile.c_str(),
                                Flags, 0, &ShaderBlob, &ErrorBlob);
    if (FAILED(Result))
    {
        if (ErrorBlob)
        {
            std::string ErrorMessage = (char*)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(ErrorMessage.c_str());

            SafeRelease(ShaderBlob);
            SafeRelease(ErrorBlob);
        }

        return nullptr;
    }

    Shader = CreateShader<ShaderClass>(ShaderBlob, nullptr);

    SafeRelease(ShaderBlob);
    SafeRelease(ErrorBlob);

    return Shader;
}

bool LoadContent()
{
    assert(g_d3dDevice);

    HRESULT Result;

    // Create an initialize the vertex buffer.
    D3D11_BUFFER_DESC VertexBufferDesc;
    ZeroMemory(&VertexBufferDesc, sizeof(D3D11_BUFFER_DESC));
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.ByteWidth = sizeof(VertexPosColor) * _countof(g_Vertices);
    VertexBufferDesc.CPUAccessFlags = 0;
    VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA ResourceData;
    ZeroMemory(&ResourceData, sizeof(D3D11_SUBRESOURCE_DATA));
    ResourceData.pSysMem = g_Vertices;
    Result = g_d3dDevice->CreateBuffer(&VertexBufferDesc, &ResourceData, &g_d3dVertexBuffer);
    if (FAILED(Result))
    {
        return false;
    }

    // Create and initialize the index buffer.
    D3D11_BUFFER_DESC IndexBufferDesc;
    ZeroMemory(&IndexBufferDesc, sizeof(D3D11_BUFFER_DESC));
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.ByteWidth = sizeof(WORD) * _countof(g_Indicies);
    IndexBufferDesc.CPUAccessFlags = 0;
    IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    ResourceData.pSysMem = g_Indicies;
    Result = g_d3dDevice->CreateBuffer(&IndexBufferDesc, &ResourceData, &g_d3dIndexBuffer);
    if (FAILED(Result))
    {
        return false;
    }

    // Create the constant buffers for the variables defined in the vertex shader.
    D3D11_BUFFER_DESC ConstantBufferDesc;
    ZeroMemory(&ConstantBufferDesc, sizeof(D3D11_BUFFER_DESC));
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantBufferDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
    ConstantBufferDesc.CPUAccessFlags = 0;
    ConstantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    Result = g_d3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Application]);
    if (FAILED(Result))
    {
        return false;
    }
    Result = g_d3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Frame]);
    if (FAILED(Result))
    {
        return false;
    }
    Result = g_d3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Object]);
    if (FAILED(Result))
    {
        return false;
    }

    // Load the shaders
    g_d3dVertexShader = LoadShader<ID3D11VertexShader>(ASSET_DIR L"/SimpleVertexShader.hlsl", "SimpleVertexShader", "latest");
    g_d3dPixelShader = LoadShader<ID3D11PixelShader>(ASSET_DIR L"/SimpleFragmentShader.hlsl", "SimpleFragmentShader", "latest");
    if (!g_d3dVertexBuffer || !g_d3dPixelShader)
    {
        return false;
    }

    // Setup the projection matrix.
    RECT ClientRect;
    GetClientRect(g_WindowHandle, &ClientRect);

    // Compute the exact client dimensions.
    // This is required for a correct projection matrix.
    float ClientWidth = static_cast<float>(ClientRect.right - ClientRect.left);
    float ClientHeight = static_cast<float>(ClientRect.bottom - ClientRect.top);
    g_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), ClientWidth / ClientHeight, 0.1f, 100.0f);
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Application], 0, nullptr, &g_ProjectionMatrix, 0, 0);

    return true;
}

void Update(float DeltaTime)
{
    DirectX::XMVECTOR EyePosition = DirectX::XMVectorSet(0, 0, -10, 1);
    DirectX::XMVECTOR FocusPoint = DirectX::XMVectorSet(0, 0, 0, 1);
    DirectX::XMVECTOR UpDirection = DirectX::XMVectorSet(0, 1, 0, 0);
    g_ViewMatrix = DirectX::XMMatrixLookAtLH(EyePosition, FocusPoint, UpDirection);
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Frame], 0, nullptr, &g_ViewMatrix, 0, 0);

    static float Angle = 0.0f;
    Angle += 90.0f * DeltaTime;
    DirectX::XMVECTOR RotationAxis = DirectX::XMVectorSet(0, 1, 1, 0);

    g_WorldMatrix = DirectX::XMMatrixRotationAxis(RotationAxis, DirectX::XMConvertToRadians(Angle));
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Object], 0, nullptr, &g_WorldMatrix, 0, 0);
}

// Clear the color and depth buffers.
void Clear(const FLOAT ClearColor[4], FLOAT ClearDepth, UINT8 ClearStencil)
{
    g_d3dDeviceContext->ClearRenderTargetView(g_d3dRenderTargetView, ClearColor);
    g_d3dDeviceContext->ClearDepthStencilView(g_d3dDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, ClearDepth, ClearStencil);
}

void Present(bool VerticalSync)
{
    if (VerticalSync)
    {
        g_d3dSwapChain->Present(1, 0);
    }
    else
    {
        g_d3dSwapChain->Present(0, 0);
    }
}

void Render()
{
    assert(g_d3dDevice);
    assert(g_d3dDeviceContext);

    Clear(DirectX::Colors::CornflowerBlue, 1.0f, 0);

    const UINT VertexStride = sizeof(VertexPosColor);
    const UINT Offset = 0;

    g_d3dDeviceContext->IASetVertexBuffers(0, 1, &g_d3dVertexBuffer, &VertexStride, &Offset);
    g_d3dDeviceContext->IASetInputLayout(g_d3dInputLayout);
    g_d3dDeviceContext->IASetIndexBuffer(g_d3dIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_d3dDeviceContext->VSSetShader(g_d3dVertexShader, nullptr, 0);
    g_d3dDeviceContext->VSSetConstantBuffers(0, 3, g_d3dConstantBuffers);

    g_d3dDeviceContext->RSSetState(g_d3dRasterizerState);
    g_d3dDeviceContext->RSSetViewports(1, &g_Viewport);

    g_d3dDeviceContext->PSSetShader(g_d3dPixelShader, nullptr, 0);

    g_d3dDeviceContext->OMSetRenderTargets(1, &g_d3dRenderTargetView, g_d3dDepthStencilView);
    g_d3dDeviceContext->OMSetDepthStencilState(g_d3dDepthStencilState, 1);

    g_d3dDeviceContext->DrawIndexed(_countof(g_Indicies), 0, 0);
}

void UnloadContent()
{
    SafeRelease(g_d3dConstantBuffers[CB_Application]);
    SafeRelease(g_d3dConstantBuffers[CB_Frame]);
    SafeRelease(g_d3dConstantBuffers[CB_Object]);
    SafeRelease(g_d3dIndexBuffer);
    SafeRelease(g_d3dVertexBuffer);
    SafeRelease(g_d3dInputLayout);
    SafeRelease(g_d3dVertexShader);
    SafeRelease(g_d3dPixelShader);
}

void Cleanup()
{
    SafeRelease(g_d3dDepthStencilView);
    SafeRelease(g_d3dRenderTargetView);
    SafeRelease(g_d3dDepthStencilBuffer);
    SafeRelease(g_d3dDepthStencilState);
    SafeRelease(g_d3dRasterizerState);
    SafeRelease(g_d3dSwapChain);
    SafeRelease(g_d3dDeviceContext);
    SafeRelease(g_d3dDevice);
}
