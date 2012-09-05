/**
 * @~english
 * @addtogroup TaoGuiInter Tao Interface Interaction
 * @ingroup TaoGui
 * Tao Presentations interface.
 * Tao Presentation graphical interface can be modified by an XL document.
 *
 * @~french
 * @addtogroup TaoGuiInter Configuration de l'interface Tao
 * @ingroup TaoGui
 * L'interface graphique de Tao Presentations peut être modifiée par le
 * document.
 *
 * @~
 * @{
 */


/**
 * @~english
 * Set the message in the status bar.
 * Show text @p t until next message.
 *
 * @~french
 * Affiche un message dans la barre d'état.
 * Le message persiste jusqu'au prochain appel, ou jusqu'à ce que Tao
 * Presentations affiche un autre message.
 */
status(t:text);

/**
 * @~english
 * Controls source code panel.
 * @p src true show the panel, @p src false hide it.
 *
 * @~french
 * Affiche ou cache la fenêtre de code source.
 * Affiche la fenêtre si @p src vaut @c true, la cache si @p src vaut
 * @c false.
 */
show_source(src:boolean);


/**
 * @~english
 * Controls error panel.
 * @p err true show the panel, @p err false hide it.
 *
 * @~french
 * Affiche ou cache la fenêtre d'erreur.
 * Affiche la fenêtre si @p err vaut @c true, la cache si @p src vaut
 * @c false.
 */
show_error(err:boolean);


/**
 * @~english
 * Controls time dependant animations.
 * @p animations true run animations, @p animations false stop them.
 * @returns [boolean] The previous value.
 *
 * @~french
 * Active ou désactive les animations.
 * Lorsque @p animations vaut @c false, le temps est virtuellement figé.
 * @returns [boolean] La valeur précédente.
 */
boolean enable_animations(animations:boolean);


/**
 * @~english
 * Controls slide-show mode.
 *
 * Activate or deactivate slide-show mode, depending on 
 * the value of the flag. When activated, the document is
 * displayed full-screen, the cursor hides automatically
 * and the screensaver is deactivated.
 * @returns [boolean] The previous value of the slide-show mode.
 *
 * @~french
 * Active ou désactive le mode présentation.
 * En mode présentation, le document est affiché en plein écran, le pointeur
 * de souris disparaît automatiquement après deux secondes d'inactivité, et
 * l'économiseur d'écran est désactivé.
 * @returns [boolean] La valeur précédente du mode présentation.
 */
boolean slide_show(show:boolean);


/**
 * @~english
 * Controls the display of rendering statistics.
 *
 * Shows or hides fps statistics.
 * @returns [boolean] The previous value.
 *
 * @~french
 * Active ou désactive les statistiques d'affichage.
 * @returns [boolean] La valeur précédente.
 */
boolean show_statistics(flag:boolean);


/**
 * @~english
 * Reset document view to default parameters.
 * The related primitives are :
 *
 * @~french
 * Réinitialise les paramètres d'affichage.
 * Les primitives concernées sont :
 * @~
 * - @ref camera_position
 * - @ref camera_target
 * - @ref camera_up_vector
 * - @ref zoom
 */
reset_view();


/**
 * @~english
 * Returns the current zoom factor.
 * @~french
 * Renvoie le facteur de zoom courant.
 */
zoom();


/**
 * @~english
 * Sets the zoom factor.
 * @~french
 * Définit le facteur de zoom.
 */
zoom( factor:real);


/**
 * @~english
 * Zooms out.
 * The zoom factor is multiplied by 0.25.
 * @~french
 * Réduit le zoom.
 * Le facteur de zoom est multiplié par 0.25.
 */
zoom_out();


/**
 * @~english
 * Zooms in.
 * The zoom factor is divided by 0.25.
 * @~french
 * Augmente le zoom.
 * Le facteur de zoom est divisé par 0.25.
 */
zoom_in();


/**
 * @~english
 * Toggle the cursor.
 * Toggle between hand (panning) and arrow (selection) cursors.
 * @~french
 * Change le curseur de souris.
 * Passe du curseur de sélection, en forme de flèche, au curseur de
 * déplacement, en forme de main.
 */
toggle_hand_cursor();


/**
 * @~english
 * Partially shows or hides elements.
 * Applies a transparency factor of @p amount to the current layout.
 * When @p amount is 0.0, the layout is totally invisible. When @p amount
 * is 1.0, the layout is totally visible.
 * @~french
 * Montre ou cache partiellement des éléments graphiques.
 * Applique un facteur de transparence @p amount au @a layout courant.
 * Lorsque @p amount vaut 0.0, le @a layout est totalement invisible. Lorsque
 * @p amount est 1.0, le @a layout est totalement visible.
 */
show(amount:real);


/**
 * @~english
 * The current value of the frame counter.
 * Returns the number of frames (images) that have been displayed so far for the
 * current document. You may use this value for instance to compute
 * frame-per-second statistics, as in the following code:
 * @~french
 * La valeur actuelle du compteur de trames.
 * Renvoie le nombre d'images qui ont été affichées depuis l'ouverture du
 * document courant. Cette primitive permet par exemple de calculer la
 * vitesse d'affichage en images par seconde, comme dans l'exemple suivant.
 * @~
 * @code
page "p1",
    goto_page "p2"

T0 := 0.0
C0 := 0
Message := "---"
FPS := 0.0
page "p2",
    locally
        if T0 = 0 then
            T0 := page_time
            C0 := frame_count

        locally
            rotatez 10*page_time
            color "blue"
            rectangle 0, 0, 200, 100

        text_box 0, -window_height/2 + 50, 300, 100,
            font "Arial", 30
            align_center
            text Message

        refresh 0.0

    locally
        if page_time > 2 then
            FPS := (frame_count-C0)/(page_time-T0)
            Message := "FPS = " & text FPS
            T0 := 0

        no_refresh_on TimerEvent
        refresh 2.0
 * @endcode
 */
integer frame_count();

/**
 * @~english
 * Opens the specified URL with an external application.
 * For example:
 * @~french
 * Ouvre l'URL avec une application externe.
 * Par exemple:
 * @~
 * @code
enable_selection_rectangle false
Clicked -> 0.0
page "URL",
    active_widget
        if page_time < Clicked + 0.1 then
            color "gray"
        else
            color "darkblue"
        rounded_rectangle 0, 0, 300, 100, 20
        URL -> "http://google.com"
        text_box 0, 0, 300, 100,
            vertical_align_center
            align_center
            color "white"
            font "Arial", 30
            text URL
        on_click
            open_url URL
            Clicked := page_time
 * @endcode
 */
boolean open_url(url:text);

/** @} */
