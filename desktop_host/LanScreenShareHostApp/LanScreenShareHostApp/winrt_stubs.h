// Stub header for WinRT types until proper C++/WinRT integration
#pragma once

// Basic stub namespaces and types to allow compilation
namespace winrt {
    namespace Windows {
        namespace Foundation {
            // Stubs for Foundation types
        }
        namespace System {
            // Stubs for System types
        }
    }
    namespace Microsoft {
        namespace UI {
            namespace Xaml {
                // Forward declare XAML types
                struct Application {};
                struct LaunchActivatedEventArgs {};
                struct Window {};
                struct UIElement {};
                namespace Controls {
                    struct Control {};
                    struct Grid {};
                    struct StackPanel {};
                    struct TextBlock {};
                    struct Button {};
                }
                namespace Navigation {
                    struct NavigationEventArgs {};
                }
                namespace Data {
                }
                namespace Input {
                }
                namespace Media {
                    namespace Imaging {
                        struct BitmapImage {};
                    }
                }
                namespace Markup {
                    struct IComponentConnector {};
                }
                namespace Documents {
                }
            }
        }
    }
}

// Avoid needing ApplicationT and other WinRT macros
#define ApplicationT(T) Application
#define WindowT(T) Window

