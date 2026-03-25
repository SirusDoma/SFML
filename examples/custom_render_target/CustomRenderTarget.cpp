////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics.hpp>

#include <SFML/Main.hpp>

#include <sstream>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdlib>


////////////////////////////////////////////////////////////
/// SpriteBatch - Collects drawable vertices into batches
/// grouped by texture, then flushes them in minimal draw
/// calls when drawn to a render target.
///
/// Inherits from both sf::RenderTarget (to receive draw
/// calls) and sf::Drawable (to be drawn to another target).
///
/// Usage:
///   SpriteBatch batch(window.getSize());
///   batch.draw(sprite1);
///   batch.draw(sprite2);
///   window.draw(batch);   // flushes batched vertices
///   batch.clear();        // explicitly clear collected data
///
////////////////////////////////////////////////////////////
class SpriteBatch : public sf::RenderTarget, public sf::Drawable
{
public:
    explicit SpriteBatch()
    {
        RenderTarget::initialize();
    }

    sf::Vector2u getSize() const override
    {
        return {};
    }

    void clear(sf::Color = sf::Color::Black) override
    {
        m_batches.clear();
        m_vboEntries.clear();
    }

    void clearStencil(sf::StencilValue) override
    {
    }

    void clear(sf::Color, sf::StencilValue) override
    {
        m_batches.clear();
        m_vboEntries.clear();
    }

    void draw(const sf::Drawable& drawable, const sf::RenderStates& states = sf::RenderStates::Default) override
    {
        sf::RenderTarget::draw(drawable, states);
    }

    void draw(const sf::Vertex*       vertices,
              std::size_t             vertexCount,
              sf::PrimitiveType       type,
              const sf::RenderStates& states) override
    {
        // Only batch triangle-based primitives
        if (type != sf::PrimitiveType::Triangles && type != sf::PrimitiveType::TriangleStrip &&
            type != sf::PrimitiveType::TriangleFan)
        {
            // Non-triangle primitives go into their own batch as-is
            Batch batch;
            batch.texture   = states.texture;
            batch.blendMode = states.blendMode;
            batch.shader    = states.shader;
            batch.type      = type;
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                sf::Vertex v = vertices[i];
                v.position   = states.transform * v.position;
                batch.vertices.push_back(v);
            }
            m_batches.push_back(std::move(batch));
            return;
        }

        // Convert TriangleStrip/TriangleFan to individual Triangles for batching
        std::vector<sf::Vertex> converted;
        if (type == sf::PrimitiveType::TriangleStrip && vertexCount >= 3)
        {
            converted.reserve((vertexCount - 2) * 3);
            for (std::size_t i = 0; i + 2 < vertexCount; ++i)
            {
                if (i % 2 == 0)
                {
                    converted.push_back(vertices[i]);
                    converted.push_back(vertices[i + 1]);
                    converted.push_back(vertices[i + 2]);
                }
                else
                {
                    converted.push_back(vertices[i + 1]);
                    converted.push_back(vertices[i]);
                    converted.push_back(vertices[i + 2]);
                }
            }
        }
        else if (type == sf::PrimitiveType::TriangleFan && vertexCount >= 3)
        {
            converted.reserve((vertexCount - 2) * 3);
            for (std::size_t i = 1; i + 1 < vertexCount; ++i)
            {
                converted.push_back(vertices[0]);
                converted.push_back(vertices[i]);
                converted.push_back(vertices[i + 1]);
            }
        }

        const sf::Vertex* srcVertices    = vertices;
        std::size_t       srcVertexCount = vertexCount;
        if (!converted.empty())
        {
            srcVertices    = converted.data();
            srcVertexCount = converted.size();
        }

        const auto& transform = states.transform;

        // Append to the current batch if render states match
        if (!m_batches.empty())
        {
            auto& current = m_batches.back();
            if (current.type == sf::PrimitiveType::Triangles && current.texture == states.texture &&
                current.blendMode == states.blendMode && current.shader == states.shader)
            {
                for (std::size_t i = 0; i < srcVertexCount; ++i)
                {
                    sf::Vertex v = srcVertices[i];
                    v.position   = transform * v.position;
                    current.vertices.push_back(v);
                }
                return;
            }
        }

        // Start a new batch
        Batch batch;
        batch.texture   = states.texture;
        batch.blendMode = states.blendMode;
        batch.shader    = states.shader;
        batch.type      = sf::PrimitiveType::Triangles;
        batch.vertices.reserve(srcVertexCount);
        for (std::size_t i = 0; i < srcVertexCount; ++i)
        {
            sf::Vertex v = srcVertices[i];
            v.position   = transform * v.position;
            batch.vertices.push_back(v);
        }
        m_batches.push_back(std::move(batch));
    }

    void draw(const sf::VertexBuffer& vertexBuffer,
              std::size_t             firstVertex,
              std::size_t             vertexCount,
              const sf::RenderStates& states) override
    {
        VBOEntry entry;
        entry.buffer      = &vertexBuffer;
        entry.firstVertex = firstVertex;
        entry.vertexCount = vertexCount;
        entry.states      = states;
        m_vboEntries.push_back(entry);
    }

    [[nodiscard]] std::size_t getBatchCount() const
    {
        return m_batches.size() + m_vboEntries.size();
    }

protected:
    void draw(sf::RenderTarget& target, sf::RenderStates) const override
    {
        for (const auto& batch : m_batches)
        {
            if (batch.vertices.empty())
                continue;

            sf::RenderStates batchStates;
            batchStates.texture   = batch.texture;
            batchStates.blendMode = batch.blendMode;
            batchStates.shader    = batch.shader;
            target.draw(batch.vertices.data(), batch.vertices.size(), batch.type, batchStates);
        }

        for (const auto& entry : m_vboEntries)
            target.draw(*entry.buffer, entry.firstVertex, entry.vertexCount, entry.states);
    }

private:
    struct Batch
    {
        const sf::Texture*      texture{};
        sf::BlendMode           blendMode{sf::BlendAlpha};
        const sf::Shader*       shader{};
        sf::PrimitiveType       type{sf::PrimitiveType::Triangles};
        std::vector<sf::Vertex> vertices;
    };

    struct VBOEntry
    {
        const sf::VertexBuffer* buffer{};
        std::size_t             firstVertex{};
        std::size_t             vertexCount{};
        sf::RenderStates        states;
    };

    std::vector<Batch>    m_batches;
    std::vector<VBOEntry> m_vboEntries;
};


////////////////////////////////////////////////////////////
/// DrawCallCounter - Collects draw calls, counts them,
/// and replays them when drawn to a render target.
///
/// Usage:
///   DrawCallCounter counter(window.getSize());
///   counter.draw(sprite1);
///   counter.draw(sprite2);
///   window.draw(counter);              // replay to window
///   counter.getDrawCallCount();        // == 2
///   counter.clear();                   // reset
///
////////////////////////////////////////////////////////////
class DrawCallCounter : public sf::RenderTarget, public sf::Drawable
{
public:
    explicit DrawCallCounter()
    {
        RenderTarget::initialize();
    }

    sf::Vector2u getSize() const override
    {
        return {};
    }

    void clear(sf::Color = sf::Color::Black) override
    {
        m_entries.clear();
        m_vboEntries.clear();
        m_drawCalls   = 0;
        m_vertexCount = 0;
    }

    void clearStencil(sf::StencilValue) override
    {
    }

    void clear(sf::Color, sf::StencilValue) override
    {
        m_entries.clear();
        m_vboEntries.clear();
        m_drawCalls   = 0;
        m_vertexCount = 0;
    }

    void draw(const sf::Drawable& drawable, const sf::RenderStates& states = sf::RenderStates::Default) override
    {
        sf::RenderTarget::draw(drawable, states);
    }

    void draw(const sf::Vertex* vertices, std::size_t vertexCount, sf::PrimitiveType type, const sf::RenderStates& states) override
    {
        ++m_drawCalls;
        m_vertexCount += vertexCount;

        VertexEntry entry;
        entry.type   = type;
        entry.states = states;
        entry.vertices.assign(vertices, vertices + vertexCount);
        m_entries.push_back(std::move(entry));
    }

    void draw(const sf::VertexBuffer& vertexBuffer, std::size_t firstVertex, std::size_t vertexCount, const sf::RenderStates& states) override
    {
        ++m_drawCalls;
        m_vertexCount += vertexCount;

        VBOEntry entry;
        entry.buffer      = &vertexBuffer;
        entry.firstVertex = firstVertex;
        entry.vertexCount = vertexCount;
        entry.states      = states;
        m_vboEntries.push_back(entry);
    }

    [[nodiscard]] std::size_t getDrawCallCount() const
    {
        return m_drawCalls;
    }

    [[nodiscard]] std::size_t getVertexCount() const
    {
        return m_vertexCount;
    }

protected:
    void draw(sf::RenderTarget& target, sf::RenderStates) const override
    {
        for (const auto& entry : m_entries)
            target.draw(entry.vertices.data(), entry.vertices.size(), entry.type, entry.states);

        for (const auto& entry : m_vboEntries)
            target.draw(*entry.buffer, entry.firstVertex, entry.vertexCount, entry.states);
    }

private:
    struct VertexEntry
    {
        sf::PrimitiveType       type{};
        sf::RenderStates        states;
        std::vector<sf::Vertex> vertices;
    };

    struct VBOEntry
    {
        const sf::VertexBuffer* buffer{};
        std::size_t             firstVertex{};
        std::size_t             vertexCount{};
        sf::RenderStates        states;
    };

    std::vector<VertexEntry>  m_entries;
    std::vector<VBOEntry>     m_vboEntries;
    std::size_t               m_drawCalls{};
    std::size_t               m_vertexCount{};
};


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "SFML Custom Render Target", sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(true);

    constexpr int numShapes = 50;

    std::vector<sf::CircleShape> circles;
    circles.reserve(numShapes);
    for (int i = 0; i < numShapes; ++i)
    {
        sf::CircleShape circle(10.f + static_cast<float>(i % 5) * 5.f);
        const auto      fi = static_cast<float>(i);
        circle.setFillColor(sf::Color(static_cast<std::uint8_t>(fi * 5),
                                      static_cast<std::uint8_t>(255 - fi * 5),
                                      static_cast<std::uint8_t>(128 + fi * 2)));
        circle.setPosition({static_cast<float>(i % 10) * 75.f + 25.f, static_cast<float>(i / 10) * 75.f + 50.f});
        circles.push_back(circle);
    }

    std::vector<sf::RectangleShape> rectangles;
    rectangles.reserve(20);
    for (int i = 0; i < 20; ++i)
    {
        sf::RectangleShape rect({30.f, 30.f});
        const auto         fi = static_cast<float>(i);
        rect.setFillColor(sf::Color(static_cast<std::uint8_t>(200 - fi * 8),
                                    static_cast<std::uint8_t>(100 + fi * 7),
                                    static_cast<std::uint8_t>(fi * 12)));
        rect.setPosition({static_cast<float>(i % 10) * 75.f + 35.f, static_cast<float>(i / 10) * 75.f + 430.f});
        rect.setRotation(sf::degrees(fi * 15.f));
        rectangles.push_back(rect);
    }

    bool useSpriteBatch = true;

    const sf::Font font("resources/tuffy.ttf");

    sf::Text statsText(font);
    statsText.setCharacterSize(14);
    statsText.setFillColor(sf::Color::White);
    statsText.setPosition({10.f, 10.f});

    sf::Text modeText(font);
    modeText.setCharacterSize(14);
    modeText.setFillColor(sf::Color::Yellow);
    modeText.setPosition({10.f, 570.f});

    SpriteBatch     batch{};
    DrawCallCounter counter{};

    while (window.isOpen())
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                    window.close();
                else if (keyPressed->code == sf::Keyboard::Key::Space)
                    useSpriteBatch = !useSpriteBatch;
            }
        }

        window.clear(sf::Color(30, 30, 30));

        if (useSpriteBatch)
        {
            // --- SpriteBatch mode ---
            // Draw entities into the batch, then draw the batch to a
            // counter, then draw the counter to the window
            batch.clear();
            counter.clear();

            for (const auto& circle : circles)
                batch.draw(circle);
            for (const auto& rect : rectangles)
                batch.draw(rect);

            const std::size_t batchCount = batch.getBatchCount();

            // Draw the batch into the counter
            counter.draw(batch);

            // Draw the counter to the window
            window.draw(counter);

            std::ostringstream ss;
            ss << "Batched mode: " << circles.size() + rectangles.size() << " shapes -> " << batchCount
               << " batches -> " << counter.getDrawCallCount() << " draw calls (" << counter.getVertexCount()
               << " vertices)";
            statsText.setString(ss.str());
            window.draw(statsText);
        }
        else
        {
            // --- Normal mode (no batching) ---
            counter.clear();

            for (const auto& circle : circles)
                counter.draw(circle);
            for (const auto& rect : rectangles)
                counter.draw(rect);

            window.draw(counter);

            std::ostringstream ss;
            ss << "Normal mode: " << circles.size() + rectangles.size() << " shapes -> "
               << counter.getDrawCallCount() << " draw calls (" << counter.getVertexCount() << " vertices)";
            statsText.setString(ss.str());
            window.draw(statsText);
        }

        modeText.setString("Press SPACE to toggle mode (" + std::string(useSpriteBatch ? "BATCHED" : "NORMAL") + ")");
        window.draw(modeText);

        window.display();
    }

    return EXIT_SUCCESS;
}
