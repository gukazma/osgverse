#include <osg/io_utils>
#include <osg/LightSource>
#include <osg/Texture2D>
#include <osg/MatrixTransform>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/ConvertUTF>
#include <osgGA/TrackballManipulator>
#include <osgUtil/CullVisitor>
#include <osgViewer/Viewer>
#include <osgViewer/CompositeViewer>
#include <osgViewer/ViewerEventHandlers>
#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>
#include <ui/ImGui.h>
#include <ui/ImGuiComponents.h>
#include <pipeline/Global.h>
#include <pipeline/IntersectionManager.h>
#include <iostream>
#include <sstream>

#ifndef _DEBUG
#include <backward.hpp>  // for better debug info
namespace backward { backward::SignalHandling sh; }
#endif
#define USE_COMPOSITE_VIEWER 0

struct MyContentHandler : public osgVerse::ImGuiContentHandler
{
    MyContentHandler(osg::Camera* camera, osg::MatrixTransform* mt)
    :   _camera(camera), _transform(mt), _rotateValue(0.0f), _rotateDir(1.0f),
        _selectID(-1), _useGizmoSnap(false)
    {
        _gizmoOperation = ImGuizmo::ROTATE;
        _gizmoMode = ImGuizmo::WORLD;
    }

    osg::observer_ptr<osg::Camera> _camera;
    osg::observer_ptr<osg::MatrixTransform> _transform;
    float _rotateValue, _rotateDir;
    int _selectID; bool _useGizmoSnap;
    ImGuizmo::OPERATION _gizmoOperation;
    ImGuizmo::MODE _gizmoMode;

    virtual void runInternal(osgVerse::ImGuiManager* mgr)
    {
        ImTextureID icon = ImGuiTextures["icon"];
        const ImGuiViewport* view = ImGui::GetMainViewport();
        ImGui::PushFont(ImGuiFonts["LXGWFasmartGothic"]);
#if 1
        int xPos = 0, yPos = 0;
        int flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                  | ImGuiWindowFlags_NoCollapse;

        setupWindow(ImVec2(view->WorkPos.x + xPos, view->WorkPos.y + yPos), ImVec2(400, 250));
        if (ImGui::Begin("Icon Buttons", NULL, flags))
        {
            ImGui::SetWindowFontScale(1.2f);
            if (ImGui::Button("Turn Left", ImVec2(120, 60))) _rotateDir = -1.0f;
            if (ImGui::Button("Stop", ImVec2(120, 60))) _rotateDir = 0.0f;
            if (ImGui::Button("Turn Right", ImVec2(120, 60))) _rotateDir = 1.0f;

            imageRotated(icon, ImVec2(280, 140), ImVec2(180, 180), _rotateValue);
            _rotateValue += _rotateDir * 0.002f;
        }
        ImGui::End(); yPos += 250;

        setupWindow(ImVec2(view->WorkPos.x + xPos, view->WorkPos.y + yPos), ImVec2(400, 250));
        if (ImGui::Begin("Transformation", NULL, flags))
        {
            osg::Matrixf matrix = _transform->getMatrix();
            ImGui::SetWindowFontScale(1.2f);
            if (editTransform(matrix, osg::Vec3(1.0f, 1.0f, 1.0f), _useGizmoSnap))
                _transform->setMatrix(matrix);
        }
        ImGui::End(); yPos += 250;

        setupWindow(ImVec2(view->WorkPos.x + xPos, view->WorkPos.y + yPos), ImVec2(400, 450));
        if (ImGui::Begin("Layers", NULL, flags))
        {
            ImGui::SetWindowFontScale(1.2f);
            if (ImGui::TreeNodeEx("Node List", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginListBox(u8"", ImVec2(360, 260)))
                {
                    for (int i = 0; i < 15; ++i)
                    {
                        if (ImGui::Selectable(("Node" + std::to_string(i)).c_str(), i == _selectID))
                        { _selectID = i; }
                    }
                    ImGui::EndListBox();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Users", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginListBox("", ImVec2(360, 120)))
                {
                    for (int i = 0; i < 5; ++i)
                    {
                        ImGui::Selectable(("User" + std::to_string(i)).c_str(), false);
                        ImGui::SameLine(200); ImGui::Text(("xArray" + std::to_string(i)).c_str());
                    }
                    ImGui::EndListBox();
                }
                ImGui::TreePop();
            }
        }
        ImGui::End();
#else
        ImGui::ShowDemoWindow();
#endif
        ImGui::PopFont();
    }

    bool editTransform(osg::Matrixf& matrix, const osg::Vec3& snapVec, bool useSnap)
    {
        if (ImGui::RadioButton("Translate", _gizmoOperation == ImGuizmo::TRANSLATE))
            _gizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", _gizmoOperation == ImGuizmo::ROTATE))
            _gizmoOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", _gizmoOperation == ImGuizmo::SCALE))
            _gizmoOperation = ImGuizmo::SCALE;

        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(matrix.ptr(), matrixTranslation, matrixRotation, matrixScale);
        ImGui::InputFloat3("m", matrixTranslation);
        ImGui::InputFloat3("deg", matrixRotation);
        ImGui::InputFloat3("", matrixScale);
        ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.ptr());

        if (_gizmoOperation != ImGuizmo::SCALE)
        {
            if (ImGui::RadioButton("Local", _gizmoMode == ImGuizmo::LOCAL))
                _gizmoMode = ImGuizmo::LOCAL;
            ImGui::SameLine();
            if (ImGui::RadioButton("Global", _gizmoMode == ImGuizmo::WORLD))
                _gizmoMode = ImGuizmo::WORLD;
        }

        ImGui::Checkbox("", &useSnap); ImGui::SameLine();
        switch (_gizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
            ImGui::InputFloat3("Snap T", (float*)snapVec.ptr()); break;
        case ImGuizmo::ROTATE:
            ImGui::InputFloat("Snap R", (float*)snapVec.ptr()); break;
        case ImGuizmo::SCALE:
            ImGui::InputFloat("Snap S", (float*)snapVec.ptr()); break;
        }

        osg::Matrixf view(_camera->getViewMatrix()), proj(_camera->getProjectionMatrix());
        ImGuiIO& io = ImGui::GetIO(); ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        return ImGuizmo::Manipulate(view.ptr(), proj.ptr(), _gizmoOperation, _gizmoMode, matrix.ptr(),
                                    NULL, (useSnap ? snapVec.ptr() : NULL));
    }

    static ImVec2 imAdd(const ImVec2& lhs, const ImVec2& rhs)
    { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }

    static ImVec2 imRotate(const ImVec2& v, float cos_a, float sin_a)
    { return ImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a); }

    static void imageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle)
    {
        ImDrawList* dList = ImGui::GetWindowDrawList();
        float cos_a = cosf(angle), sin_a = sinf(angle);
        ImVec2 pos[4] =
        {
            imAdd(center, imRotate(ImVec2(-size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a)),
            imAdd(center, imRotate(ImVec2(+size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a)),
            imAdd(center, imRotate(ImVec2(+size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a)),
            imAdd(center, imRotate(ImVec2(-size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a))
        };
        ImVec2 uvs[4] = { ImVec2(0.0f, 0.0f), ImVec2(1.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec2(0.0f, 1.0f) };
        dList->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0], uvs[1], uvs[2], uvs[3], IM_COL32_WHITE);
    }

    static void setupWindow(const ImVec2& pos, const ImVec2& size, float alpha = 0.6f)
    {
        ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowBgAlpha(alpha);
    }
};

class InteractiveHandler : public osgGA::GUIEventHandler
{
public:
    InteractiveHandler(osgVerse::ImGuiManager* imgui, osg::Drawable* d)
    : _imgui(imgui), _drawable(d) {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
        osgViewer::View* view = static_cast<osgViewer::View*>(&aa);
        osgVerse::IntersectionResult result = osgVerse::findNearestIntersection(
            view->getCamera(), ea.getXnormalized(), ea.getYnormalized());
        if (result.drawable != _drawable.get()) return false;
        if (result.intersectTextureData.empty()) return false;

        const osgVerse::IntersectionResult::IntersectTextureData& td = result.intersectTextureData[0];
        osg::Vec2 uv = osg::Vec2(td.second[0], 1.0f - td.second[1]);
        switch (ea.getEventType())
        {
        case osgGA::GUIEventAdapter::DOUBLECLICK:
        case osgGA::GUIEventAdapter::PUSH:
            _imgui->setMouseInput(uv, ea.getButton(), 0.0f); break;
        case osgGA::GUIEventAdapter::RELEASE:
        case osgGA::GUIEventAdapter::DRAG:
            _imgui->setMouseInput(uv, ea.getButtonMask(), 0.0f); break;
        case osgGA::GUIEventAdapter::MOVE:
            _imgui->setMouseInput(uv, 0, 0.0f); break;
        case osgGA::GUIEventAdapter::SCROLL:
            _imgui->setMouseInput(uv, ea.getButtonMask(),
                (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP ? 1.0f : -1.0f));
            break;
        }
        return false;
    }

protected:
    osg::observer_ptr<osgVerse::ImGuiManager> _imgui;
    osg::observer_ptr<osg::Drawable> _drawable;
};

int main(int argc, char** argv)
{
    bool guiAsTexture = false;
    osgVerse::globalInitialize(argc, argv);
#if USE_COMPOSITE_VIEWER
    osgViewer::CompositeViewer viewer;
#else
    osgViewer::Viewer viewer;
#endif

    osg::ref_ptr<osg::Node> scene =
        (argc < 2) ? osgDB::readNodeFile("cessna.osg") : osgDB::readNodeFile(argv[1]);
    if (!scene) { OSG_WARN << "Failed to load " << (argc < 2) ? "" : argv[1]; return 1; }

    // The scene graph
    osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform;
    root->addChild(scene.get());

    // The ImGui setup
    osg::ref_ptr<osgVerse::ImGuiManager> imgui = new osgVerse::ImGuiManager;
    imgui->setChineseSimplifiedFont(MISC_DIR + "LXGWFasmartGothic.otf");
    imgui->setGuiTexture("icon", "Images/osg128.png");

#if USE_COMPOSITE_VIEWER
    for (int v = 0; v < 3; ++v)
    {
        osg::ref_ptr<osgViewer::View> view = new osgViewer::View;
        view->addEventHandler(new osgViewer::StatsHandler);
        view->addEventHandler(new osgViewer::WindowSizeHandler);
        view->setCameraManipulator(new osgGA::TrackballManipulator);
        view->setSceneData(root.get());
        view->setUpViewInWindow(640 * v, 20, 640, 640);
        viewer.addView(view.get());

        if (v > 0) continue;  // ImGUI cannot working on multiple views at present
        imgui->initialize(new MyContentHandler(view->getCamera(), root.get()), false);
        imgui->addToView(view);
    }
#else
    imgui->initialize(new MyContentHandler(viewer.getCamera(), root.get()), guiAsTexture);
    if (guiAsTexture)
    {
        osg::Texture* guiTex = imgui->addToTexture(root.get(), 1920, 1080);
        osg::Geometry* quad = osg::createTexturedQuadGeometry(
            osg::Vec3(), osg::X_AXIS * 16.0f, osg::Z_AXIS * 9.0f);
        quad->getOrCreateStateSet()->setTextureAttributeAndModes(0, guiTex);
        quad->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::ON);
        quad->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        viewer.addEventHandler(imgui->getHandler());
        viewer.addEventHandler(new InteractiveHandler(imgui.get(), quad));

        osg::Geode* geode = new osg::Geode;
        geode->addDrawable(quad); root->addChild(geode);
    }
    else
        imgui->addToView(&viewer);

    viewer.addEventHandler(new osgViewer::StatsHandler);
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);
    viewer.setCameraManipulator(new osgGA::TrackballManipulator);
    viewer.setSceneData(root.get());
    viewer.setUpViewOnSingleScreen(0);
#endif
    viewer.setThreadingModel(osgViewer::ViewerBase::SingleThreaded);
    return viewer.run();
}
