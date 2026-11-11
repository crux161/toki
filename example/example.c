#include <chaitea.h>
#include <stdio.h>

typedef struct
{
    strbuf buf;
    const char* name;
} Model;

/**
 * Initialize the model and return a first command.
 *
 * If `init` does nothing like this, you can actually pass NULL to `chai_new`.
 */
Command init(App* app, void* model) { return CHAI_IGNORED; }

/**
 * Update the model based on a events.
 *
 * `update` is called everytime there is a message to treat.
 *
 * `update` returns commands that can change control the program.
 */
Command update(App* app, void* model, Message msg)
{
    (void)app;
    Model* state = model;

    switch (msg.kind) {
        case MSG_KEY_DOWN:
            switch (msg.data.key.code) {
                case 'q':
                case T_KEY_ESC:
                    return CHAI_QUIT;
            }
            break;
        default: break;
    }

    // if returning CHAI_IGNORED, the view will not be redrawn
    return CHAI_IGNORED;
}

/**
 * Produce a string representing the app based on the model.
 *
 * The user still owns the memory and can free it in `cleanup`.
 */
View view(App* app, void* model)
{
    Model* m = model;

    // bprintf is like sprintf but it grows the buffer on the fly
    bprintf(&m->buf, "Hello, %s!", m->name);

    return view_text(app, ASARRAY(str, m->buf));
}

/**
 * Free memory allocated in `view`.
 *
 * If you use static memory and `cleanup` has nothing to do, you can
 * instead pass NULL to `chai_new`.
 */
void cleanup(App* app, void* model)
{
    Model* m = model;
    CLEAR(m->buf);
}

int main(void)
{
    // create a new model with some data (name)
    // and a scratch buffer used in frames (buf)
    // zeroing the scratch buffer is initialization
    Model model = { .name = "World", .buf = {0} };

    // initialize the app, setting the callbacks
    // `update` and `view` are mandatory
    // `init` and `cleanup` are optional and can be NULL
    App* app = chai_new(&model, init, update, view, cleanup);

    // run the app
    Error err = chai_run(app);
    if (err.kind != ERR_NONE) {
        // print the error
        str s = chai_err_msg(err);
        printf("Error: %.*s\n", (int)s.len, s.data);
        exit(1);
    }

    return 0;
}
