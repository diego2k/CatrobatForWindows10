﻿//
// DirectXPage.xaml.cpp
// Implementation of the DirectXPage class.
//

#include "pch.h"
#include "DirectXPage.xaml.h"

using namespace Catrobat_Player;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Phone::UI::Input;
using namespace concurrency;
using namespace D2D1;

//----------------------------------------------------------------------

DirectXPage::DirectXPage() :
m_windowVisible(true),
m_coreInput(nullptr)
{
    InitializeComponent();

    // Register event handlers for page lifecycle.
    CoreWindow^ window = Window::Current->CoreWindow;

    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &DirectXPage::OnVisibilityChanged);

    DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

    currentDisplayInformation->DpiChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDpiChanged);

    currentDisplayInformation->OrientationChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnOrientationChanged);

    DisplayInformation::DisplayContentsInvalidated +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDisplayContentsInvalidated);

    swapChainPanel->CompositionScaleChanged +=
        ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &DirectXPage::OnCompositionScaleChanged);

    swapChainPanel->SizeChanged +=
        ref new SizeChangedEventHandler(this, &DirectXPage::OnSwapChainPanelSizeChanged);

    // register hardware back button event
    HardwareButtons::BackPressed +=
        ref new EventHandler<BackPressedEventArgs^>(this, &DirectXPage::OnHardwareBackButtonPressed);

    // At this point we have access to the device. 
    // We can create the device-dependent resources.
    m_deviceResources = std::make_shared<DX::DeviceResources>();
    m_deviceResources->SetSwapChainPanel(swapChainPanel);

    // Register our SwapChainPanel to get independent input pointer events
    auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^)
    {
        // The CoreIndependentInputSource will raise pointer events for the specified device types on whichever thread it's created on.
        m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
            Windows::UI::Core::CoreInputDeviceTypes::Mouse |
            Windows::UI::Core::CoreInputDeviceTypes::Touch |
            Windows::UI::Core::CoreInputDeviceTypes::Pen
            );

        // Register for pointer events, which will be raised on the background thread.
        m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerPressed);

        // Begin processing input messages as they're delivered.
        m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    });

    // Run task on a dedicated high priority background thread.
    m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

    m_main = std::unique_ptr<Catrobat_PlayerMain>(new Catrobat_PlayerMain(m_deviceResources, PlayerAppBar));
    m_main->StartRenderLoop();
}

//----------------------------------------------------------------------

DirectXPage::~DirectXPage()
{
    // Stop rendering and processing events on destruction.
    m_main->StopRenderLoop();
    m_coreInput->Dispatcher->StopProcessEvents();
}

//----------------------------------------------------------------------
// Saves the current state of the app for suspend and terminate events.

void DirectXPage::SaveInternalState(IPropertySet^ state)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->Trim();

    // Stop rendering when the app is suspended.
    m_main->StopRenderLoop();

    // Put code to save app state here.
}

//----------------------------------------------------------------------
// Loads the current state of the app for resume events.

void DirectXPage::LoadInternalState(IPropertySet^ state)
{
    // Put code to load app state here.

    // Start rendering when the app is resumed.
    m_main->StartRenderLoop();
}

//----------------------------------------------------------------------
// Window event handlers.

void DirectXPage::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
    m_windowVisible = args->Visible;
    if (m_windowVisible)
    {
        m_main->StartRenderLoop();
    }
    else
    {
        m_main->StopRenderLoop();
    }
}

//----------------------------------------------------------------------
// DisplayInformation event handlers.

void DirectXPage::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetDpi(sender->LogicalDpi);
    m_main->CreateWindowSizeDependentResources();
}

//----------------------------------------------------------------------

void DirectXPage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
    m_main->CreateWindowSizeDependentResources();
}

//----------------------------------------------------------------------

void DirectXPage::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->ValidateDevice();
}

//----------------------------------------------------------------------

void DirectXPage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
    m_main->PointerPressed(Point2F(e->CurrentPoint->Position.X, e->CurrentPoint->Position.Y));
}

//----------------------------------------------------------------------

void DirectXPage::OnHardwareBackButtonPressed(_In_ Platform::Object^ sender, BackPressedEventArgs ^args)
{
    m_main->HardwareBackButtonPressed(sender, args);
}

//----------------------------------------------------------------------

void DirectXPage::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
    m_main->CreateWindowSizeDependentResources();
}

//----------------------------------------------------------------------

void DirectXPage::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    critical_section::scoped_lock lock(m_main->GetCriticalSection());
    m_deviceResources->SetLogicalSize(e->NewSize);
    m_main->CreateWindowSizeDependentResources();
}

//----------------------------------------------------------------------

void DirectXPage::OnRestartButtonClicked(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    m_main->RestartButtonClicked(sender, args);
}

//----------------------------------------------------------------------

void DirectXPage::OnPlayButtonClicked(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    m_main->PlayButtonClicked(sender, args);
}

//----------------------------------------------------------------------

void DirectXPage::OnThumbnailButtonClicked(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    m_main->ThumbnailButtonClicked(sender, args);
}

//----------------------------------------------------------------------

void DirectXPage::OnEnableAxisButtonClicked(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    m_main->EnableAxisButtonClicked(sender, args);
}

//----------------------------------------------------------------------

void DirectXPage::OnScreenshotButtonClicked(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    m_main->ScreenshotButtonClicked(sender, args);
}


//void Catrobat_Player::DirectXPage::PlayerAppBar_Opened(Platform::Object^ sender, Platform::Object^ e)
//{
//
//}
