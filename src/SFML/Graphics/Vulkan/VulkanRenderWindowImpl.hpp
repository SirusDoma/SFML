////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2026 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/RenderWindowImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>

#include <SFML/Window/WindowHandle.hpp>

#include <chrono>
#include <vector>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Vulkan implementation of the render window presentation surface
///
////////////////////////////////////////////////////////////
class VulkanRenderWindowImpl : public RenderWindowImpl, public VulkanRenderSurface
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, creates the swapchain for a window
    ///
    /// \param device       Graphics device presenting to the window
    /// \param handle       Handle of the window to present to
    /// \param settings     Requested settings of the presentation surface
    /// \param bitsPerPixel Requested pixel depth, unused, the swapchain format is fixed
    ///
    ////////////////////////////////////////////////////////////
    VulkanRenderWindowImpl(VulkanGraphicsDevice&  device,
                           WindowHandle           handle,
                           const ContextSettings& settings,
                           unsigned int           bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~VulkanRenderWindowImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Present the rendered frame on screen
    ///
    ////////////////////////////////////////////////////////////
    void present() override;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable vertical synchronization
    ///
    /// \param enabled `true` to enable v-sync, `false` to deactivate it
    ///
    ////////////////////////////////////////////////////////////
    void setVerticalSyncEnabled(bool enabled) override;

    ////////////////////////////////////////////////////////////
    /// \brief Resize the presentation surface
    ///
    /// \param size New size of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    void resize(Vector2u size) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the settings of the presentation surface
    ///
    /// \return Settings actually used by the surface
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const ContextSettings& getSettings() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the surface uses sRGB encoding
    ///
    /// \return `true` if the surface uses sRGB encoding
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isSrgb() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the surface as the current render target
    ///
    /// \param active `true` to activate, `false` to deactivate
    ///
    /// \return `true` on success, `false` on failure
    ///
    ////////////////////////////////////////////////////////////
    bool activate(bool active) override;

    ////////////////////////////////////////////////////////////
    /// \brief Provide the attachments of the next render pass
    ///
    /// A swapchain image is acquired when none is held yet.
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if the attachments are ready to be drawn into
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool prepareAttachments(VulkanSurfaceAttachments& attachments) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get an image holding the window's rendered content
    ///
    /// \param size Filled with the size of the returned image
    ///
    /// \return Image with the rendered content, null when the window cannot be read
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImage acquireReadableColorImage(Vector2u& size) override;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Create or re-create the swapchain and its resources
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool createSwapchain();

    ////////////////////////////////////////////////////////////
    /// \brief Destroy and re-create the presentation surface
    ///
    /// Last-resort recovery: drivers exist that keep failing every
    /// swapchain operation for a surface after certain window
    /// transitions until the surface itself is re-created.
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool recreateSurface();

    ////////////////////////////////////////////////////////////
    /// \brief Destroy the swapchain resources
    ///
    /// \param keepSwapchain Keep the swapchain handle alive for re-creation
    ///
    ////////////////////////////////////////////////////////////
    void destroySwapchainResources(bool keepSwapchain);

    ////////////////////////////////////////////////////////////
    /// \brief Create the multisample and depth-stencil images
    ///
    ////////////////////////////////////////////////////////////
    void createAncillaryImages();

    ////////////////////////////////////////////////////////////
    /// \brief Pick the present mode for the current v-sync state and intent
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkPresentModeKHR choosePresentMode() const;

    ////////////////////////////////////////////////////////////
    /// \brief Watch whether v-synced presentation is actually paced
    ///
    /// A v-synced present cycle can never legitimately complete
    /// faster than the display's refresh period. Broken driver
    /// states exist that execute v-synced presents immediately;
    /// sustained impossibly fast cycles are reported once. FIFO
    /// presentation is already the most compatible path Vulkan
    /// has, so unlike Direct3D there is no other path to fall
    /// back to.
    ///
    /// \param presentResult Result of the present call of this cycle
    ///
    ////////////////////////////////////////////////////////////
    void monitorPresentationPacing(VkResult presentResult);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    VulkanGraphicsDevice& m_device;   //!< Graphics device presenting to the window
    WindowHandle          m_handle{}; //!< Window the swapchain presents to

    VkSurfaceKHR   m_surface{};   //!< Presentation surface of the window
    VkSwapchainKHR m_swapchain{}; //!< Swapchain presenting to the window

    std::vector<VkImage>     m_images;            //!< Images of the swapchain, owned by it
    std::vector<VkImageView> m_imageViews;        //!< Views of the swapchain images
    std::vector<VkSemaphore> m_renderFinished;    //!< Per-image semaphores presentation waits on
    std::vector<VkSemaphore> m_acquireSemaphores; //!< Ring of semaphores image acquisition signals
    std::size_t              m_acquireIndex{};    //!< Next acquire semaphore to use
    std::uint32_t            m_imageIndex{};      //!< Swapchain image currently held
    bool                     m_acquired{};        //!< Whether a swapchain image is currently held
    bool                     m_minimized{};       //!< Whether the window is minimized, nothing can present then
    bool                     m_needsRecreate{};   //!< Whether the swapchain went out of date mid-frame

    VkImage       m_multisampleImage{};       //!< Multisampled color image, null without anti-aliasing
    VmaAllocation m_multisampleAllocation{};  //!< Allocation backing the multisampled image
    VkImageView   m_multisampleView{};        //!< View of the multisampled image
    VkImage       m_depthStencilImage{};      //!< Depth-stencil image, can be null
    VmaAllocation m_depthStencilAllocation{}; //!< Allocation backing the depth-stencil image
    VkImageView   m_depthStencilView{};       //!< View of the depth-stencil image

    VkFormat        m_format{VK_FORMAT_B8G8R8A8_UNORM};              //!< Format of the swapchain images
    VkColorSpaceKHR m_colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}; //!< Color space paired with the format
    VkFormat        m_depthStencilFormat{}; //!< Format of the depth-stencil image, undefined when none
    Vector2u        m_extent;               //!< Size of the swapchain images
    unsigned int    m_sampleCount{1};       //!< Samples per pixel of the rendering

    std::vector<VkPresentModeKHR> m_availablePresentModes; //!< Present modes the surface supports
    ContextSettings               m_settings;              //!< Settings actually used by the surface
    ContextSettings::Presentation m_presentation{};        //!< Requested presentation intent
    bool                          m_vsync{};               //!< Whether v-sync is enabled
    bool                          m_sRgb{};                //!< Whether the swapchain uses sRGB encoding
    bool                          m_readable{};            //!< Whether the swapchain images can be copied from
    bool                          m_suboptimal{};          //!< Whether an acquire reported a drifted swapchain

    std::uint64_t m_presentId{}; //!< Monotonic id handed to each present for present-wait pacing

    std::chrono::steady_clock::time_point m_lastPresentTime; //!< When the previous present cycle completed
    unsigned int m_unsyncedPresentStreak{}; //!< Consecutive v-synced cycles that completed impossibly fast
    bool         m_pacingWarned{};          //!< Whether the broken pacing warning was already printed
};

} // namespace sf::priv
