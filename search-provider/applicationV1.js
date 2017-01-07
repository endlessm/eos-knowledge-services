const Lang = imports.lang;
const Gio = imports.gi.Gio;

let SearchProvider = imports.searchProviderV1;

let Application = new Lang.Class({
    Name: 'EksSearchProviderV1',
    Extends: Gio.Application,

    _init: function() {
        this.parent({
            application_id: 'com.endlessm.EknServices.SearchProviderV1',
            flags: Gio.ApplicationFlags.IS_SERVICE,
            inactivity_timeout: 12000,
        });

        this._search_provider = new SearchProvider.GlobalSearchProvider();
    },

    vfunc_dbus_register: function(connection, path) {
        this.parent(connection, path);
        this._search_provider.register(connection, path);
        return true;
    },

    vfunc_dbus_unregister: function(connection, path) {
        this.parent(connection, path);
        this._search_provider.unregister(connection);
    },
});
