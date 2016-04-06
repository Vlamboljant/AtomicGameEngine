//
// Copyright (c) 2014-2016 THUNDERBEAST GAMES LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Atomic/Container/ArrayPtr.h>
#include <Atomic/UI/UI.h>
#include <Atomic/IO/Log.h>
#include <Atomic/IO/File.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Resource/JSONFile.h>
#include <Atomic/Resource/ResourceEvents.h>

#include <Atomic/Core/CoreEvents.h>
#include <AtomicJS/Javascript/JSVM.h>

#include <ToolCore/ToolEnvironment.h>

#include <AtomicWebView/WebViewEvents.h>
#include <AtomicWebView/UIWebView.h>
#include <AtomicWebView/WebClient.h>
#include <AtomicWebView/WebMessageHandler.h>
#include <AtomicWebView/WebTexture2D.h>

#include "JSResourceEditor.h"


using namespace tb;
using namespace ToolCore;

namespace AtomicEditor
{

JSResourceEditor ::JSResourceEditor(Context* context, const String &fullpath, UITabContainer *container) :
    ResourceEditor(context, fullpath, container)
{

    TBLayout* layout = new TBLayout();
    layout->SetLayoutSize(LAYOUT_SIZE_GRAVITY);
    layout->SetGravity(WIDGET_GRAVITY_ALL);
    layout->SetLayoutDistribution(LAYOUT_DISTRIBUTION_GRAVITY);

    rootContentWidget_->GetInternalWidget()->AddChild(layout);

    TBContainer* c = new TBContainer();
    c->SetGravity(WIDGET_GRAVITY_ALL);

    layout->AddChild(c);

    ToolEnvironment* tenv = GetSubsystem<ToolEnvironment>();
    String codeEditorDir = tenv->GetToolDataDir();
    codeEditorDir += "CodeEditor/Editor.html";

#ifdef ATOMIC_PLATFORM_OSX
    String url = "file://" + codeEditorDir;
#else
    String url = "file:///" + codeEditorDir;
#endif

    webView_ = new UIWebView(context_, url);
    webClient_ = webView_->GetWebClient();
    messageHandler_ = new WebMessageHandler(context_);
    webClient_->AddMessageHandler(messageHandler_);

    webView_->GetWebTexture2D()->SetClearColor(Color(.23f, .23f, .23f, 1));

    SubscribeToEvent(webClient_, E_WEBVIEWLOADEND, HANDLER(JSResourceEditor, HandleWebViewLoadEnd));
    SubscribeToEvent(messageHandler_, E_WEBMESSAGE, HANDLER(JSResourceEditor, HandleWebMessage));

    SubscribeToEvent(E_RENAMERESOURCENOTIFICATION, HANDLER(JSResourceEditor, HandleRenameResourceNotification));
    SubscribeToEvent(E_DELETERESOURCENOTIFICATION, HANDLER(JSResourceEditor, HandleDeleteResourceNotification));
    SubscribeToEvent(E_PROJECTUNLOADEDNOTIFICATION, HANDLER(JSResourceEditor, HandleProjectUnloadedNotification));

    c->AddChild(webView_->GetInternalWidget());

}
    
JSResourceEditor::~JSResourceEditor()
{

}
   
String getNormalizedPath(const String& path)
{
    // Full path is the fully qualified path from the root of the filesystem.  In order
    // to take advantage of the resource caching system, let's trim it down to just the
    // path inside the resources directory including the Resources directory so that the casing
    // is correct.
    const String& RESOURCES_MARKER = "resources/";
    return path.SubstringUTF8(path.ToLower().Find(RESOURCES_MARKER));
}
    
void JSResourceEditor::HandleRenameResourceNotification(StringHash eventType, VariantMap& eventData)
{
    using namespace RenameResourceNotification;
    const String& newPath = eventData[P_NEWRESOURCEPATH].GetString();
    const String& path = eventData[P_RESOURCEPATH].GetString();
    
    webClient_->ExecuteJavaScript(ToString("HOST_resourceRenamed(\"%s\",\"%s\");", getNormalizedPath(path).CString(), getNormalizedPath(newPath).CString()));
    
    if (fullpath_.Compare(path) == 0) {
        fullpath_ = newPath;
        SetModified(modified_);
    }
}
    
void JSResourceEditor::HandleDeleteResourceNotification(StringHash eventType, VariantMap& eventData)
{
    using namespace DeleteResourceNotification;
    const String& path = eventData[P_RESOURCEPATH].GetString();
    
    webClient_->ExecuteJavaScript(ToString("HOST_resourceDeleted(\"%s\");", getNormalizedPath(path).CString()));
}
    
void JSResourceEditor::HandleProjectUnloadedNotification(StringHash eventType, VariantMap& eventData)
{
    webClient_->ExecuteJavaScript("HOST_projectUnloaded();");
}
    
void JSResourceEditor::HandleWebViewLoadEnd(StringHash eventType, VariantMap& eventData)
{
    // need to wait until we get an editor load complete message since we could
    // still be streaming things in.
}

void JSResourceEditor::HandleWebMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace WebMessage;

    const String& request = eventData[P_REQUEST].GetString();
    const String& EDITOR_CHANGE = "editorChange";
    const String& EDITOR_SAVE_CODE = "editorSaveCode";
    const String& EDITOR_SAVE_FILE = "editorSaveFile";
    const String& EDITOR_LOAD_COMPLETE = "editorLoadComplete";
    
    String normalizedPath = getNormalizedPath(fullpath_);
    
    WebMessageHandler* handler = static_cast<WebMessageHandler*>(eventData[P_HANDLER].GetPtr());

    if (request == EDITOR_CHANGE)
    {
        SetModified(true);
    }
    else if (request == EDITOR_LOAD_COMPLETE)
    {
        // We need to wait until the editor javascript is all required in to call the
        // method to load the code.  The HandleWebViewLoadEnd event is getting called
        // too soon.
        webClient_->ExecuteJavaScript(ToString("HOST_loadCode(\"atomic://%s\");", normalizedPath.CString()));
    }
    else
    {
        JSONValue jvalue;
        if (JSONFile::ParseJSON(request, jvalue, false))
        {
            String message = jvalue["message"].GetString();
            if (message == EDITOR_SAVE_CODE)
            {
                String code = jvalue["payload"].GetString();
                File file(context_, fullpath_, FILE_WRITE);
                file.Write((void*) code.CString(), code.Length());
                file.Close();
            }
            else if (message == EDITOR_SAVE_FILE)
            {
                String code = jvalue["payload"].GetString();
                String fn = jvalue["filename"].GetString();
                // TODO: determine if we are absolute path or partial path
                File file(context_, fn, FILE_WRITE);
                file.Write((void*) code.CString(), code.Length());
                file.Close();
            }
        }
    }

    handler->Success();

}

void JSResourceEditor::FormatCode()
{
    //webClient_->ExecuteJavaScript("beautifyCode();");
}

bool JSResourceEditor::OnEvent(const TBWidgetEvent &ev)
{
    if (ev.type == EVENT_TYPE_SHORTCUT)
    {
        if (ev.ref_id == TBIDC("close"))
        {
            RequestClose();
        }
    }

    return false;
}

void JSResourceEditor::FindTextClose()
{
}

bool JSResourceEditor::FindText(const String& findText, unsigned flags)
{

    return true;
}

void JSResourceEditor::SetFocus()
{
    //editField_->SetFocus(WIDGET_FOCUS_REASON_UNKNOWN);
}

void JSResourceEditor::GotoTokenPos(int tokenPos)
{

}

void JSResourceEditor::GotoLineNumber(int lineNumber)
{

}

bool JSResourceEditor::Save()
{
    if (!modified_)
        return true;

    webClient_->ExecuteJavaScript("HOST_saveCode();");

    SetModified(false);

    return true;

}


}
