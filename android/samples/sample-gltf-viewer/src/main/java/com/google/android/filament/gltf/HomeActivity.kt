package com.google.android.filament.gltf

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.ClickableText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.google.android.filament.gltf.fw.FWMainActivity

class HomeActivity : ComponentActivity() {
    private lateinit var resultLauncher: ActivityResultLauncher<Intent>
    private val selectionResetVersion = mutableIntStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        resultLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result: ActivityResult ->
            logg(
                "activityResult",
                "resultCode", result.resultCode,
                "data", result.data ?: "null"
            )
            if (result.resultCode == RESULT_OK) {
                selectionResetVersion.intValue++
            }
        }

        // Get all .glb files from assets/models directory
        val modelFiles = try {
            getModelFilesRecursive()
        } catch (e: Exception) {
            e.printStackTrace()
            emptyList()
        }

        setContentView(
            ComposeView(this).apply {
                setContent {
                    val collapsedDirectoriesState = remember { mutableStateOf(setOf<String>()) }

                    LazyColumn(
                        modifier = Modifier.padding(32.dp),
                    ) {
                        val resetKey = selectionResetVersion.value

                        addItems(resetKey, modelFiles, collapsedDirectoriesState)
                    }
                }
            }
        )
    }

    private fun LazyListScope.addItems(
        resetKey: Int,
        modelFiles: List<String>,
        collapsedDirectoriesState: MutableState<Set<String>>
    ) {
        val directoryMap = modelFiles.groupBy { it.substringBefore('/', missingDelimiterValue = "") }
        directoryMap.forEach { (directory, files) ->
            if (directory.isEmpty()) {
                addRootLevelFiles(resetKey, files)
            } else {
                addNonRootLevelFiles(resetKey, directory, collapsedDirectoriesState, files)
            }
        }
    }

    private fun LazyListScope.addNonRootLevelFiles(
        resetKey: Int,
        directory: String,
        collapsedDirectoriesState: MutableState<Set<String>>,
        files: List<String>
    ) {
        item {
            ClickableText(
                modifier = Modifier.padding(16.dp),
                text = AnnotatedString(directory),
                style = TextStyle.Default.copy(color = Color.White, fontWeight = FontWeight.Bold),
                onClick = {
                    val current = collapsedDirectoriesState.value
                    collapsedDirectoriesState.value = if (current.contains(directory)) {
                        current - directory
                    } else {
                        current + directory
                    }
                }
            )
        }

        if (collapsedDirectoriesState.value.contains(directory).not()) {
            items(
                items = files,
                itemContent = { modelName ->
                    ModelText(resetKey, modelName, AnnotatedString(modelName.substringAfter('/')))
                }
            )
        }
    }

    @Composable
    private fun ModelText(resetKey: Int, modelName: String, text: AnnotatedString) {
        var isSelected by remember(resetKey) { mutableStateOf(false) }

        val backgroundColor = if (isSelected) Color.Black else Color.Transparent

        Box(
            modifier = Modifier
                .background(backgroundColor)
                .fillMaxWidth()
                .clickable {
                    isSelected = true
                    val intent = Intent(this@HomeActivity, FWMainActivity::class.java).apply {
                        putExtra("model", modelName)
                    }
                    resultLauncher.launch(intent)
                },
            contentAlignment = Alignment.CenterStart,
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 6.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                BasicText(
                    modifier = Modifier.padding(start = 32.dp),
                    text = text,
                    style = TextStyle.Default.copy(color = Color.White),
                )
            }
        }
    }

    private fun LazyListScope.addRootLevelFiles(resetKey: Int, files: List<String>) {
        item {
            BasicText(
                modifier = Modifier.padding(16.dp),
                text = AnnotatedString("root"),
                style = TextStyle.Default.copy(color = Color.White, fontWeight = FontWeight.Bold),
            )
        }
        items(
            items = files,
            itemContent = { modelName ->
                ModelText(resetKey, modelName, AnnotatedString(modelName))
            }
        )
    }

    private fun getModelFilesRecursive(path: String = "models"): List<String> {
        val result = mutableListOf<String>()
        val files = assets.list(path) ?: return result
        for (file in files) {
            val fullPath = if (path.isEmpty()) file else "$path/$file"
            if (file.endsWith(".glb")) {
                result.add(fullPath.removePrefix("models/").removeSuffix(".glb"))
            } else {
                result.addAll(getModelFilesRecursive(fullPath))
            }
        }

        return result.sorted()
    }
}
