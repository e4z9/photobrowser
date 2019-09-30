# Best Practice Patterns

## Listening

You will listen to changes in cells/streams to update properties in non-FRP code, e.g. Qt UI.

* Use `ensureSameThread` for updating UI and most other things.

* Do not use `sample` in `listen` - it gets the old value instead of the new one.
  You _can_ use `sample` if you postpone it via `post` and therefore get out of the FRP transaction.

## Reacting to and reporting changes

You will need to send values to FRP cells and streams when changes occur in non-FRP code,
e.g. Qt UI.

* If you are sure that the change can not be triggered through code that might originate from FRP,
  call `send` directly. For example on `QComboBox::activated` or `QTreeView::doubleClicked`.

* If the change might be triggered through less controlled sources, use `post` to get the `send`
  out of any potentially running FRP transaction.
