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

import * as EditorEvents from "../editor/EditorEvents";
import * as EditorUI from "../ui/EditorUI";


/**
 * Generic registry for storing Editor Extension Services
 */
class ServiceRegistry<T extends Editor.Extensions.EditorService> implements Editor.Extensions.ServiceRegistry<T> {
    registeredServices: T[] = [];

    /**
     * Adds a service to the registered services list for this type of service
     * @param  {T}      service the service to register
     */
    register(service: T) {
        this.registeredServices.push(service);
    }

}

interface ServiceEventSubscriber {
    /**
     * Allow this service registry to subscribe to events that it is interested in
     * @param  {Atomic.UIWidget} topLevelWindow The top level window that will be receiving these events
     */
    subscribeToEvents(topLevelWindow: Atomic.UIWidget);
}

/**
 * Registry for service extensions that are concerned about project events
 */
export class ProjectServiceRegistry extends ServiceRegistry<Editor.HostExtensions.ProjectService> implements ServiceEventSubscriber {
    constructor() {
        super();
    }

    /**
     * Allow this service registry to subscribe to events that it is interested in
     * @param  {Atomic.UIWidget} topLevelWindow The top level window that will be receiving these events
     */
    subscribeToEvents(eventDispatcher: Editor.Extensions.EventDispatcher) {
        eventDispatcher.subscribeToEvent(EditorEvents.LoadProjectNotification, (ev) => this.projectLoaded(ev));
        eventDispatcher.subscribeToEvent(EditorEvents.CloseProject, (ev) => this.projectUnloaded(ev));
        eventDispatcher.subscribeToEvent(EditorEvents.PlayerStartRequest, () => this.playerStarted());
    }

    /**
     * Called when the project is unloaded
     * @param  {[type]} data Event info from the project unloaded event
     */
    projectUnloaded(data) {
        this.registeredServices.forEach((service) => {
            // Notify services that the project has been unloaded
            try {
                if (service.projectUnloaded) {
                    service.projectUnloaded();
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n \n ${e.stack}`);
            }
        });
    }

    /**
     * Called when the project is loaded
     * @param  {[type]} data Event info from the project unloaded event
     */
    projectLoaded(ev: Editor.EditorEvents.LoadProjectEvent) {
        this.registeredServices.forEach((service) => {
            try {
                // Notify services that the project has just been loaded
                if (service.projectLoaded) {
                    service.projectLoaded(ev);
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n \n ${e.stack}`);
            }
        });
    }

    playerStarted() {
        this.registeredServices.forEach((service) => {
            try {
                // Notify services that the project has just been loaded
                if (service.playerStarted) {
                    service.playerStarted();
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n \n ${e.stack}`);
            }
        });
    }
}

/**
 * Registry for service extensions that are concerned about Resources
 */
export class ResourceServiceRegistry extends ServiceRegistry<Editor.HostExtensions.ResourceService> {
    constructor() {
        super();
    }

    /**
     * Allow this service registry to subscribe to events that it is interested in
     * @param  {Atomic.UIWidget} topLevelWindow The top level window that will be receiving these events
     */
    subscribeToEvents(eventDispatcher: Editor.Extensions.EventDispatcher) {
        eventDispatcher.subscribeToEvent(EditorEvents.SaveResourceNotification, (ev) => this.saveResource(ev));
        eventDispatcher.subscribeToEvent(EditorEvents.DeleteResourceNotification, (ev) => this.deleteResource(ev));
        eventDispatcher.subscribeToEvent(EditorEvents.RenameResourceNotification, (ev) => this.renameResource(ev));
    }

    /**
     * Called after a resource has been saved
     * @param  {Editor.EditorEvents.SaveResourceEvent} ev
     */
    saveResource(ev: Editor.EditorEvents.SaveResourceEvent) {
        // run through and find any services that can handle this.
        this.registeredServices.forEach((service) => {
            try {
                // Verify that the service contains the appropriate methods and that it can save
                if (service.save) {
                    service.save(ev);
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n \n ${e.stack}`);
            }
        });
    }

    /**
     * Called when a resource has been deleted
     */
    deleteResource(ev: Editor.EditorEvents.DeleteResourceEvent) {
        this.registeredServices.forEach((service) => {
            try {
                // Verify that the service contains the appropriate methods and that it can delete
                if (service.delete) {
                    service.delete(ev);
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n ${e}\n ${e.stack}`);
            }
        });
    }

    /**
     * Called when a resource has been renamed
     * @param  {Editor.EditorEvents.RenameResourceEvent} ev
     */
    renameResource(ev: Editor.EditorEvents.RenameResourceEvent) {
        this.registeredServices.forEach((service) => {
            try {
                // Verify that the service contains the appropriate methods and that it can handle the rename
                if (service.rename) {
                    service.rename(ev);
                }
            } catch (e) {
                EditorUI.showModalError("Extension Error", `Error detected in extension ${service.name}\n \n ${e.stack}`);
            }
        });
    }

}
