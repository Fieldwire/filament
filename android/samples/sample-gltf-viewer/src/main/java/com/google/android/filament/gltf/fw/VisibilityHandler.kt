package com.google.android.filament.gltf.fw

import com.google.android.filament.Entity
import com.google.android.filament.utils.ModelViewer

class VisibilityHandler(private val modelViewer: FWModelViewer) {

    var hiddenEntitiesCount = 0
        private set

    fun handle(action: Action) {
        when (action) {
            is Action.Hide -> hideEntity(action.entity)
            is Action.Isolate -> isolateEntity(action.entity)
            Action.ShowAll -> showAllEntities()
        }
    }

    private fun isolateEntity(@Entity selectedEntity: Int) {
        val asset = modelViewer.asset ?: return
        val rm = modelViewer.engine.renderableManager

        // Hide all entities
        asset.renderableEntities.forEach { entity ->
            val instance = rm.getInstance(entity)
            // select: 1 is visibility bit
            // value: 0 hides the entity, 1 shows the entity
            rm.setLayerMask(instance, 1, 0)
        }

        // Show only the selectedEntity
        rm.setLayerMask(rm.getInstance(selectedEntity), 1, 1)

        // Since all other entities are hidden when an entity is isolated
        // override the hiddenEntities count
        hiddenEntitiesCount = asset.renderableEntities.size - 1
    }

    private fun hideEntity(@Entity entity: Int) {
        val rm = modelViewer.engine.renderableManager
        rm.setLayerMask(rm.getInstance(entity), 1, 0)

        // This also covers the case where an entity is first isolated and then hidden
        hiddenEntitiesCount++
    }

    private fun showAllEntities() {
        val asset = modelViewer.asset ?: return
        val rm = modelViewer.engine.renderableManager

        asset.renderableEntities.forEach { entity ->
            val instance = rm.getInstance(entity)
            // select: 1 is visibility bit
            // value: 0 hides the entity, 1 shows the entity
            rm.setLayerMask(instance, 1, 1)
        }

        // Now all entities are displayed, reset the hidden count
        hiddenEntitiesCount = 0
    }

    sealed interface Action {
        class Isolate(@Entity val entity: Int) : Action
        class Hide(@Entity val entity: Int) : Action
        object ShowAll : Action
    }
}
