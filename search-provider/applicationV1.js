const Eknc = imports.gi.EosKnowledgeContent;
const Lang = imports.lang;
const Gio = imports.gi.Gio;

let SearchProvider = imports.searchProviderV1;

// https://www.freedesktop.org/software/systemd/man/sd_bus_path_encode.html
function systemd_bus_path_decode(string) {
    return string.replace(/_([a-zA-Z0-9]{2})/g, function(m, a) {
        return String.fromCharCode(parseInt(a, 16));
    });
}

let Application = new Lang.Class({
    Name: 'EksSearchProviderV1',
    Extends: Gio.Application,

    _init: function() {
        this.parent({
            application_id: 'com.endlessm.EknServices.SearchProviderV1',
            flags: Gio.ApplicationFlags.IS_SERVICE,
            inactivity_timeout: 12000,
        });

        this._dispatcher = new Eknc.SubtreeDispatcher({ interface_info: SearchProvider.SearchIfaceInfo });
        this._dispatcher.connect('dispatch-subtree', Lang.bind(this, this._dispatchSubtree));
        this._appSearchProviders = {};
    },

    vfunc_dbus_register: function(connection, path) {
        this.parent(connection, path);
        this._dispatcher.register(connection, path);
        return true;
    },

    vfunc_dbus_unregister: function(connection, path) {
        this.parent(connection, path);
        this._dispatcher.unregister();
    },

    _dispatchSubtree: function(dispatcher, subnode) {
        if (this._appSearchProviders[subnode])
            return this._appSearchProviders[subnode].skeleton;

        let app_id = systemd_bus_path_decode(subnode);
        let provider = new SearchProvider.AppSearchProvider({ application_id: app_id });
        this._appSearchProviders[subnode] = provider;
        return provider.skeleton;
    },
});
