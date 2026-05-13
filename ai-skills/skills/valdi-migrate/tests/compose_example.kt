// Jetpack Compose example — @Composable functions, remember, LaunchedEffect,
// LazyColumn, navigation, CompositionLocal.
// Goal: migrate this to Valdi.

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.navigation.NavController

// --- CompositionLocal theme ---
val LocalAppTheme = compositionLocalOf { AppTheme() }
data class AppTheme(val primary: String = "#FFFC00")

// --- Stateless greeting ---
@Composable
fun Greeting(name: String) {
    Text("Hello, $name")
}

// --- Stateful counter ---
@Composable
fun Counter(label: String) {
    var count by remember { mutableStateOf(0) }
    Button(onClick = { count++ }) {
        Text("$label: $count")
    }
}

// --- Data loading with LaunchedEffect ---
@Composable
fun UserProfile(userId: String) {
    var profile by remember { mutableStateOf<Profile?>(null) }

    LaunchedEffect(userId) {
        profile = fetchProfile(userId)   // suspend fun
    }

    if (profile == null) {
        CircularProgressIndicator()
    } else {
        Text(profile!!.name)
    }
}

// --- LazyColumn list ---
@Composable
fun UserList(users: List<User>) {
    LazyColumn {
        items(users, key = { it.id }) { user ->
            UserRow(user = user)
        }
    }
}

// --- Layout with Column / Row / Modifier ---
@Composable
fun ProfileCard(user: User) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp)
    ) {
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(user.name, style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.weight(1f))
            Text(user.handle)
        }
        Spacer(modifier = Modifier.height(8.dp))
        Text(user.bio)
    }
}

// --- Navigation ---
@Composable
fun HomeScreen(navController: NavController) {
    Button(onClick = { navController.navigate("detail/42") }) {
        Text("Go to Detail")
    }
}

// --- CompositionLocal consumer ---
@Composable
fun ThemedBadge(text: String) {
    val theme = LocalAppTheme.current
    Box(
        modifier = Modifier.background(Color(android.graphics.Color.parseColor(theme.primary)))
    ) {
        Text(text)
    }
}
